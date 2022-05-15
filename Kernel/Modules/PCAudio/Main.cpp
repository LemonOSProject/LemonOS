#include "Audio.h"

#include "AC97.h"
#include "HDAudio.h"

#include <Module.h>

#include <IDT.h>
#include <IOPorts.h>
#include <Math.h>
#include <Memory.h>
#include <PCI.h>
#include <PCIVendors.h>
#include <Pair.h>
#include <Thread.h>

namespace Audio {
static const Pair<uint16_t, uint16_t> controllerPCIIDs[] = {
    {PCI::VendorAMD, 0x1457},   {PCI::VendorAMD, 0x1487},   {PCI::VendorIntel, 0x1c20}, {PCI::VendorIntel, 0x1d20},
    {PCI::VendorIntel, 0x8c20}, {PCI::VendorIntel, 0x8ca0}, {PCI::VendorIntel, 0x8d20}, {PCI::VendorIntel, 0x8d21},
    {PCI::VendorIntel, 0xa1f0}, {PCI::VendorIntel, 0xa270}, {PCI::VendorIntel, 0x9c20}, {PCI::VendorIntel, 0x9c21},
    {PCI::VendorIntel, 0x9ca0}, {PCI::VendorIntel, 0xa170}, {PCI::VendorIntel, 0x9d70}, {PCI::VendorIntel, 0xa171},
    {PCI::VendorIntel, 0x9d71}, {PCI::VendorIntel, 0xa2f0}, {PCI::VendorIntel, 0xa348}, {PCI::VendorIntel, 0x9dc8},
    {PCI::VendorIntel, 0x02c8}, {PCI::VendorIntel, 0x06c8}, {PCI::VendorIntel, 0xf1c8}, {PCI::VendorIntel, 0xa3f0},
    {PCI::VendorIntel, 0xf0c8}, {PCI::VendorIntel, 0x34c8}, {PCI::VendorIntel, 0x3dc8}, {PCI::VendorIntel, 0x38c8},
    {PCI::VendorIntel, 0x4dc8}, {PCI::VendorIntel, 0xa0c8}, {PCI::VendorIntel, 0x43c8}, {PCI::VendorIntel, 0x490d},
    {PCI::VendorIntel, 0x7ad0}, {PCI::VendorIntel, 0x51c8}, {PCI::VendorIntel, 0x4b55}, {PCI::VendorIntel, 0x4b58},
    {PCI::VendorIntel, 0x5a98}, {PCI::VendorIntel, 0x1a98}, {PCI::VendorIntel, 0x3198}, {PCI::VendorIntel, 0x0a0c},
    {PCI::VendorIntel, 0x0c0c}, {PCI::VendorIntel, 0x0d0c}, {PCI::VendorIntel, 0x160c}, {PCI::VendorIntel, 0x3b56},
    {PCI::VendorIntel, 0x811b}, {PCI::VendorIntel, 0x080a}, {PCI::VendorIntel, 0x0f04}, {PCI::VendorIntel, 0x2284},
    {PCI::VendorIntel, 0x2668}, {PCI::VendorIntel, 0x27d8}, {PCI::VendorIntel, 0x269a}, {PCI::VendorIntel, 0x284b},
    {PCI::VendorIntel, 0x293e}, {PCI::VendorIntel, 0x293f}, {PCI::VendorIntel, 0x3a3e}, {PCI::VendorIntel, 0x3a6e},
};

static Vector<AudioController*>* audioControllers;

IntelHDAudioController::IntelHDAudioController(const PCIInfo& info) : PCIDevice(info) {
    SetDeviceName("Intel HD Audio Controller");

    uintptr_t bar = GetBaseAddressRegister(0);
    cRegs = reinterpret_cast<ControllerRegs*>(Memory::KernelAllocate4KPages(4));

    Memory::KernelMapVirtualMemory4K(bar, (uintptr_t)cRegs, 4);

    cRegs->globalControl = 1; // Set CRST bit to leave reset mode
    while (!(cRegs->globalControl & 0x1))
        ;

    Log::Info("[HDAudio] Initializing Intel HD Audio Controller (Base: %x, Virtual Base: %x, Global Control: %x)", bar,
              cRegs, cRegs->globalControl);
}

void AC97IRQHandler(void* ctx, RegisterContext*) { ((AC97Controller*)ctx)->OnIRQ(); }

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

    /*outportw(m_ioPort + NAMReset, 1);
    while (inportw(m_ioPort + NAMReset))
        ;*/

    outportw(m_ioPort + NAMMasterVolume, AC97_MIXER_VOLUME(AC97_VOLUME_MAX, AC97_VOLUME_MAX, 0));
    outportw(m_ioPort + NAMPCMVolume, AC97_MIXER_VOLUME(AC97_VOLUME_MAX, AC97_VOLUME_MAX, 0));

    m_pcmEncoding = SoundEncoding::PCMS16LE;

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
    assert(OutputGetEncoding(nullptr) == SoundEncoding::PCMS16LE);

    m_pcmSampleSize = 2;
    if (m_pcmEncoding == SoundEncoding::PCMS20LE) {
        m_pcmSampleSize = 4; // 20-bit samples take up a dword
    } else
        assert(m_pcmEncoding == SoundEncoding::PCMS16LE);

    unsigned i = AC97_BDL_ENTRIES;
    while (i--) {
        KernelAllocateMappedBlock<uint16_t>(&sampleBuffersPhys[i], &sampleBuffers[i]);

        bufferDescriptorList[i] = BufferDescriptor{
            .address = (uint32_t)sampleBuffersPhys[i],
            // At two channels, 16-bit samples this is 4096/2/2 = 1024 samples/channel
            .sampleCount = (uint16_t)(PAGE_SIZE_4K / m_pcmSampleSize),
            .flags = 0
        };
    }

    outportl(m_nabmPort + PO_BufferDescriptorList, bufferDescriptorListPhys);
    
    uint8_t irq = AllocateVector(PCIVectorAny);
    IDT::RegisterInterruptHandler(irq, AC97IRQHandler, this);

    uint32_t globalControl = inportl(m_nabmPort + NBGlobalControl);
    globalControl = AC97_CONTROL_PCM_OUT_MODE_16BIT(globalControl);
    globalControl = AC97_CONTROL_PCM_OUT_2_CHANNELS(globalControl);
    globalControl |= NBGlobalInterruptEnable;

    outportl(m_nabmPort + NBGlobalControl, globalControl);

    uint8_t transferControl = inportb(nabmTransferControl);
    transferControl |= NBLastEntryInterrupt | NBInterruptOnCompletion | NBFIFOErrorInterrupt;
    outportb(nabmTransferControl, transferControl);

    RegisterPCMOut(m_pcm);
    StopDMA();
}

int AC97Controller::SetMasterVolume(int percentage) {
    int volume = AC97_VOLUME_MIN - (AC97_VOLUME_MIN * percentage / 100);
    outportw(m_ioPort + NAMMasterVolume, AC97_MIXER_VOLUME(volume, AC97_VOLUME_MAX, 0));
    return 0;
}

int AC97Controller::GetMasterVolume() const {
    return (inportw(m_ioPort + NAMMasterVolume) & AC97_MIXER_VOLUME_RIGHT_MASK) * 100 / AC97_VOLUME_MIN;
}

int AC97Controller::OutputSetVolume(void* output, int percentage) {
    int volume = AC97_VOLUME_MIN - (AC97_VOLUME_MIN * percentage / 100);
    outportw(m_ioPort + NAMPCMVolume, AC97_MIXER_VOLUME(volume, AC97_VOLUME_MAX, 0));
    return 0;
}

int AC97Controller::OutputGetVolume(void* output) const {
    return (inportw(m_ioPort + NAMPCMVolume) & AC97_MIXER_VOLUME_RIGHT_MASK) * 100 / AC97_VOLUME_MIN;
}

SoundEncoding AC97Controller::OutputGetEncoding(void* output) const { return SoundEncoding::PCMS16LE; }

int AC97Controller::OutputNumberOfChannels(void* output) const { return 2; }

int AC97Controller::OutputSampleRate(void* output) const { return 48000; }

int AC97Controller::OutputSetNumberOfChannels(int channels) { return -ENOSYS; }

int AC97Controller::WriteSamples(void* output, uint8_t* buffer, size_t size) {
    // TODO: Ensure exclusive access to /dev/pcm,
    // currently if two threads attempt to play audio directly
    // they will deadlock

    if(size & m_pcmSampleSize * m_pcmNumChannels) {
        return -EINVAL; // Must be writing exact frame
    }

    unsigned samplesToWrite = size / m_pcmSampleSize;
    unsigned buffersToWrite = ((size + PAGE_SIZE_4K - 1) >> PAGE_SHIFT_4K);
    
    while (size > 0)
    {
        uint8_t isDMARunning = !(inportw(m_nabmPort + PO_TransferStatus) & NBDMAStatus);
        int timer = 4000;
        while(isDMARunning && timer > 0) {
            Timer::Wait(1);
            isDMARunning = !(inportw(m_nabmPort + PO_TransferStatus) & NBDMAStatus);

            timer--;
        }

        if(timer <= 0) {
            Log::Warning("AC97 DMA still running after 4 seconds, stopping");
            StopDMA();
        }

        assert(inportw(m_nabmPort + PO_TransferStatus) & NBDMAStatus);

        {
            // Disable interrupts in case IRQ fires
            ScopedSpinLock<true> lockController(m_lock);
        
            uint8_t currentBuffer = inportb(m_nabmPort + PO_CurrentEntry);
            uint8_t lastValidEntry = inportb(m_nabmPort + PO_LastValidEntry);

            assert("DMA has stopped," && currentBuffer == lastValidEntry);

            int count = MIN(buffersToWrite, AC97_BDL_ENTRIES);
            while(count--) {
                size_t written = MIN(PAGE_SIZE_4K, size);
                Log::Info("Writing %d/%d bytes", written, size);
                memcpy(sampleBuffers[lastValidEntry], buffer, written);
                bufferDescriptorList[lastValidEntry].flags = 0;

                buffer += written;
                size -= written;

                buffersToWrite--;

                // Stop on the last entry
                if(count) {
                    lastValidEntry = (lastValidEntry + 1) % AC97_BDL_ENTRIES;
                }
            }

            bufferDescriptorList[lastValidEntry].flags |= BDLastEntry | BDInterruptOnCompletion;

            outportb(m_nabmPort + PO_LastValidEntry, lastValidEntry);
            StartDMA();
        }

        timer = 400;
        while(timer--) {
            uint8_t isDMARunning = !(inportw(m_nabmPort + PO_TransferStatus) & NBDMAStatus);
            if(!isDMARunning) {
                break;
            }

            // Sleep for 10ms
            Thread::Current()->Sleep(10000);
        }

        if(timer <= 0) {
            Log::Warning("AC97 DMA still running after 4 seconds, stopping");
            StopDMA();
            return -EIO;
        }
    }

    return samplesToWrite;
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

int ModuleInit() {
    audioControllers = new Vector<AudioController*>();

    /*for (auto& p : controllerPCIIDs) {
        PCI::EnumeratePCIDevices(p.item2, p.item1, [](const PCIInfo& info) -> void {
            IntelHDAudioController* cnt = new IntelHDAudioController(info);

            DeviceManager::RegisterDevice(cnt);
            audioControllers->add_back(cnt);
        });
    }*/

    PCI::EnumerateGenericPCIDevices(PCI_CLASS_MULTIMEDIA, PCI_SUBCLASS_AC97, [](const PCIInfo& info) -> void {
        AC97Controller* cnt = new AC97Controller(info);

        DeviceManager::RegisterDevice(cnt);
        audioControllers->add_back(cnt);
    });

    Log::Info("[PCAudio] Found controllers: %d", audioControllers->get_length());

    if (audioControllers->get_length() <= 0) {
        return 1; // No controllers present, module can be unloaded
    }

    return 0;
}

int ModuleExit() {
    for (auto& cnt : *audioControllers) {
        delete cnt;
    }
    audioControllers->clear();

    delete audioControllers;
    return 0;
}

DECLARE_MODULE("pcaudio", "AC97 and Intel HD Audio Controller Driver", ModuleInit, ModuleExit);
} // namespace Audio
