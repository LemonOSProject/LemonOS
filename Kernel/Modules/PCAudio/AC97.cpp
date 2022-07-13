#include "AC97.h"

#include <IDT.h>
#include <IOPorts.h>
#include <Math.h>

namespace Audio {

AC97Controller::AC97Controller(const PCIInfo& info) : PCIDevice(info) {
    SetDeviceName("AC97 Audio Controller");

    uintptr_t bar = GetBaseAddressRegister(0);
    assert(BarIsIOPort(0));

    m_ioPort = bar;

    m_busMasterBaseAddress = GetBaseAddressRegister(1);
    assert(BarIsIOPort(1));

    EnableBusMastering();
    EnableMemorySpace();
    EnableIOSpace();

    m_pcm = new PCMOutput{this, nullptr};

    m_nabmPort = m_busMasterBaseAddress;

    outportw(m_ioPort + NAMMasterVolume, AC97_MIXER_VOLUME(AC97_VOLUME_MAX, AC97_VOLUME_MAX, 0));
    outportw(m_ioPort + NAMPCMVolume, AC97_MIXER_VOLUME(AC97_VOLUME_MAX, AC97_VOLUME_MAX, 0));

    uint16_t nabmTransferControl = m_nabmPort + PO_TransferControl;

    // Reset PCM out
    outportb(nabmTransferControl, inportb(nabmTransferControl) | NBTransferReset);
    while (inportb(nabmTransferControl) & NBTransferReset)
        ;

    // Buffer descriptor list is 32 entries
    // Pointer is 32-bit
    bufferDescriptorListPhys = Memory::AllocatePhysicalMemoryBlock();
    assert(bufferDescriptorListPhys < 0xFFFFFFFF);
    bufferDescriptorList = (BufferDescriptor*)Memory::GetIOMapping(bufferDescriptorListPhys);
    static_assert(sizeof(BufferDescriptor) * 32 < PAGE_SIZE_4K);

    memset(bufferDescriptorList, 0, PAGE_SIZE_4K);

    assert(OutputNumberOfChannels(nullptr) == 2);

    uint32_t globalControl = inportl(m_nabmPort + NBGlobalControl);
    globalControl = AC97_CONTROL_PCM_OUT_MODE_16BIT(globalControl);
    globalControl = AC97_CONTROL_PCM_OUT_2_CHANNELS(globalControl);
    globalControl |= NBGlobalInterruptEnable;

    // QEMU does not support 20-bit audio
    // uint32_t globalStatus = inportl(m_nabmPort + NBGlobalStatus);
    // assert(AC97_STATUS_SAMPLE_FORMATS(globalStatus) == 1);

    m_pcmEncoding = SoundEncoding::PCMS16LE;

    m_pcmSampleSize = 2;
    if (m_pcmEncoding == SoundEncoding::PCMS20LE) {
        m_pcmSampleSize = 4; // 20-bit samples take up a dword
    } else
        assert(m_pcmEncoding == SoundEncoding::PCMS16LE);

    unsigned i = AC97_BDL_ENTRIES;
    while (i--) {
        KernelAllocateMappedBlock<uint16_t>(&sampleBuffersPhys[i], &sampleBuffers[i]);

        bufferDescriptorList[i] =
            BufferDescriptor{.address = (uint32_t)sampleBuffersPhys[i],
                             // At two channels, 16-bit samples this is 4096/2/2 = 1024 samples/channel
                             .sampleCount = (uint16_t)(PAGE_SIZE_4K / m_pcmSampleSize),
                             .flags = 0};
    }
    m_samplesPerBuffer = PAGE_SIZE_4K / m_pcmSampleSize / m_pcmNumChannels;

    outportl(m_nabmPort + PO_BufferDescriptorList, bufferDescriptorListPhys);
    outportl(m_nabmPort + NBGlobalControl, globalControl);

    uint8_t transferControl = inportb(nabmTransferControl);
    transferControl |= NBLastEntryInterrupt | NBInterruptOnCompletion | NBFIFOErrorInterrupt;
    outportb(nabmTransferControl, transferControl);

    // Not really a percentage of audio volume (yet)
    // as volume is in deibels
    OutputSetVolume(nullptr, 85);

    RegisterPCMOut(m_pcm);
    StopDMA();
}

int AC97Controller::SetMasterVolume(int percentage) {
    int volume = AC97_VOLUME_MIN - (AC97_VOLUME_MIN * percentage / 100);
    outportw(m_ioPort + NAMMasterVolume, AC97_MIXER_VOLUME(volume, volume, 0));
    return 0;
}

int AC97Controller::GetMasterVolume() const {
    return (inportw(m_ioPort + NAMMasterVolume) & AC97_MIXER_VOLUME_RIGHT_MASK) * 100 / AC97_VOLUME_MIN;
}

int AC97Controller::OutputSetVolume(void* output, int percentage) {
    // Not really a percentage because volume is in db
    int volume = AC97_VOLUME_MIN - (AC97_VOLUME_MIN * percentage / 100);
    outportw(m_ioPort + NAMPCMVolume, AC97_MIXER_VOLUME(volume, volume, 0));
    return 0;
}

int AC97Controller::OutputGetVolume(void* output) const {
    return (inportw(m_ioPort + NAMPCMVolume) & AC97_MIXER_VOLUME_RIGHT_MASK) * 100 / AC97_VOLUME_MIN;
}

SoundEncoding AC97Controller::OutputGetEncoding(void* output) const { return m_pcmEncoding; }

int AC97Controller::OutputNumberOfChannels(void* output) const { return 2; }

int AC97Controller::OutputSampleRate(void* output) const { return AC97_SAMPLE_RATE; }

int AC97Controller::OutputSetNumberOfChannels(int channels) { return -ENOSYS; }

int AC97Controller::WriteSamples(void* output, uint8_t* buffer, size_t size, bool async) {
    // TODO: Ensure exclusive access to /dev/pcm,
    // currently if two threads attempt to play audio directly
    // they may deadlock
    if (size % (m_pcmSampleSize * m_pcmNumChannels)) {
        Log::Info("[AC97] invalid write: not an exact frame");
        return -EINVAL; // Must be writing exact frame
    }

    int totalSamplesWritten = 0;
    int buffersToWrite = (((int)size + PAGE_SIZE_4K - 1) >> PAGE_SHIFT_4K);

    while (size > 0) {
        uint8_t isDMARunning = !(inportw(m_nabmPort + PO_TransferStatus) & NBDMAStatus);
        if (isDMARunning) {
            int remainingBuffers = 0;
            do {
                int currentBuffer = inportb(m_nabmPort + PO_CurrentEntry);
                int lastValidEntry = inportb(m_nabmPort + PO_LastValidEntry);

                // Get the remaining buffers to be processed
                remainingBuffers = lastValidEntry - currentBuffer;
                if (remainingBuffers < 0) {
                    remainingBuffers += AC97_BDL_ENTRIES;
                }
                remainingBuffers += 1;

                if (remainingBuffers >= AC97_BDL_ENTRIES - 1) {
                    // We can not fit any buffers,
                    // Wait for the samples to finish processing, give 10ms leeway
                    long samples =
                        bufferDescriptorList[(currentBuffer + 1) % AC97_BDL_ENTRIES].sampleCount / m_pcmNumChannels;

                    // Sleep whilst the next buffer gets processed
                    if (samples > 0) {
                        Thread::Current()->Sleep(samples * 1000000 / AC97_SAMPLE_RATE);
                    }
                }
            } while (remainingBuffers >= AC97_BDL_ENTRIES - 1 && IsDMARunning());
        }

        int samplesWritten = 0;
        {
            ScopedSpinLock lockController(m_lock);

            int currentBuffer = inportb(m_nabmPort + PO_CurrentEntry);
            int lastValidEntry = inportb(m_nabmPort + PO_LastValidEntry);

            int nextBuffer = lastValidEntry % AC97_BDL_ENTRIES;
            isDMARunning = !(inportw(m_nabmPort + PO_TransferStatus) & NBDMAStatus);
            if (isDMARunning) {
                nextBuffer = (lastValidEntry + 1) % AC97_BDL_ENTRIES;
                if (nextBuffer == currentBuffer) {
                    continue;
                }
            }

            do {
                int written = MIN(PAGE_SIZE_4K, size);
                // Copy audio data to our mapped buffers which will be sent
                // to the hardware
                memcpy(sampleBuffers[nextBuffer], buffer, written);
                bufferDescriptorList[nextBuffer].flags = 0;

                samplesWritten += written / m_pcmSampleSize / m_pcmNumChannels;

                // Increment the source buffer
                buffer += written;
                size -= written;

                // Make sure we update this value in case the buffer isnt full
                // to make sure the AC97 doesn't play old audio
                bufferDescriptorList[nextBuffer].sampleCount = written / m_pcmSampleSize;
                buffersToWrite--;

                nextBuffer = (nextBuffer + 1) % AC97_BDL_ENTRIES;
            } while (buffersToWrite-- && nextBuffer != currentBuffer);

            outportb(m_nabmPort + PO_LastValidEntry, nextBuffer - 1);

            isDMARunning = !(inportw(m_nabmPort + PO_TransferStatus) & NBDMAStatus);
            if (!isDMARunning) {
                StartDMA(); // Ensure DMA is running
            }
        }
        totalSamplesWritten += samplesWritten;
    }

    return totalSamplesWritten;
}

void AC97Controller::OnIRQ() {
    Log::Info("AC97 IRQ!!");

    ScopedSpinLock lockController(m_lock);
    uint16_t status = inportw(m_ioPort + PO_TransferStatus);

    if (status & NBInterruptOnCompletion) {
        Log::Info("IOC");
    } else if (status & NBLastEntryInterrupt) {
        Log::Info("End of transfer!!");
        StopDMA();
    }

    // Clear all flags
    outportw(m_ioPort + PO_TransferStatus, 0xffff);
}
} // namespace Audio