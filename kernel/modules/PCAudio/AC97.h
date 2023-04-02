#pragma once

#include "Audio.h"

#include <IOPorts.h>
#include <PCI.h>
#include <RingBuffer.h>

#define AC97_VOLUME_MAX 0
#define AC97_VOLUME_MIN 63

#define AC97_MIXER_VOLUME(right, left, mute) ((uint16_t)((right & 0x3f) | ((left & 0x3f) << 8) | (mute & 1 << 15)))
#define AC97_MIXER_VOLUME_RIGHT_MASK 0x3f

#define AC97_CONTROL_PCM_OUT_CHANNELS_MASK ((0x3U) << 20)
// 2 channels is 0
#define AC97_CONTROL_PCM_OUT_2_CHANNELS(val) (val & ~AC97_CONTROL_PCM_OUT_CHANNELS_MASK)

#define AC97_CONTROL_PCM_OUT_MODE_MASK ((0x3U) << 22)
// 16 bit samples is 0, 20 is 1
#define AC97_CONTROL_PCM_OUT_MODE_16BIT(val) (val & ~AC97_CONTROL_PCM_OUT_MODE_MASK)
#define AC97_CONTROL_PCM_OUT_MODE_20BIT(val) ((val & ~AC97_CONTROL_PCM_OUT_MODE_MASK) | (1U << 22))

#define AC97_STATUS_SAMPLE_FORMATS(val) ((val >> 22) & 0x3)

// amount of buffer descriptor list entries
#define AC97_BDL_ENTRIES 32

#define AC97_SAMPLE_RATE 48000

namespace Audio {

class AC97Controller : public AudioController, public PCIDevice {
public:
    AC97Controller(const PCIInfo* info);

    int SetMasterVolume(int percentage) override;
    int GetMasterVolume() const override;

    int OutputSetVolume(void* output, int percentage) override;
    int OutputGetVolume(void* output) const override;
    SoundEncoding OutputGetEncoding(void* output) const override;
    // The audio system will expect interleaved samples,
    // e.g. for dual channel audio the first sample will be for left channel,
    // second sample for right, third for left, etc.
    int OutputNumberOfChannels(void* output) const override;
    int OutputSampleRate(void* output) const override;
    int OutputSetNumberOfChannels(int channels) override;

    int WriteSamples(void* output, UIOBuffer* buffer, size_t size, bool async) override;

    void OnIRQ();

private:
    inline void StartDMA() {
        outportb(m_nabmPort + PO_TransferControl, inportb(m_nabmPort + PO_TransferControl) | NBTransferDMAControl);
    }

    inline void StopDMA() {
        outportb(m_nabmPort + PO_TransferControl, inportb(m_nabmPort + PO_TransferControl) & ~NBTransferDMAControl);
    }

    inline uint8_t IsDMARunning() const {
        return !(inportw(m_nabmPort + PO_TransferStatus) & NBDMAStatus);
    }

    // Native Audio Bus Master Registers
    struct NABMBufferDescriptor {
        uint32_t pAddr; // Physical addr of buffer
        uint16_t numSamples; // Number of samples in buffer
        uint16_t resvd : 13;
        uint16_t isLastEntry : 1; // Is last entry in buffer
        uint16_t sendInterrupt : 1; // Fire interrupt when processed
    } __attribute__((packed));

    enum NABMRegisters {
        NBPCMInBox = 0,
        NBPCMOutBox = 0x10,
        NBMicrophoneBox = 0x20,
        NBGlobalControl = 0x30,
        NBGlobalStatus = 0x34,
    };

    // Under the spec in AC97_x_...
    enum NABMRegisterBoxOffsets {
        RBBufferDescriptorList = 0, // 32b
        // Current buffer descriptor entry (ring buffer read pointer)
        RBCurrentEntry = 0x4,
        // Last valid buffer descriptor entry (ring buffer write pointer)
        RBLastValidEntry = 0x5,
        RBTransferStatus = 0x6, // 16b
        RBNumTransferredSamples = 0x8, // 16b
        RBNumNextProcessedEntry = 0xA,
        RBTransferControl = 0xB,

        // PCM Out 
        PO_BufferDescriptorList = NBPCMOutBox + 0, // 32b
        PO_CurrentEntry = NBPCMOutBox + 0x4,
        PO_LastValidEntry = NBPCMOutBox + 0x5,
        PO_TransferStatus = NBPCMOutBox + 0x6, // 16b
        PO_NumTransferredSamples = NBPCMOutBox + 0x8, // 16b
        PO_NumNextProcessedEntry = NBPCMOutBox + 0xA,
        PO_TransferControl = NBPCMOutBox + 0xB, 

    };

    enum NativeAudioMixerReigsters {
        NAMReset = 0,
        NAMMasterVolume= 0x2,
        NAMPCMVolume = 0x18,
    };

    enum NABMGlobalControlFlags {
        NBGlobalInterruptEnable = 0x1,
        NBColdReset = 0x2,
        NBWarmReset = 0x4,
        NBShutdown = 0x8,
    };

    enum NABMTransferStatusFlags {
        NBDMAStatus = 1,
        NBEndOfTransfer = 2,
        // WC (write clear)
        NBLastEntryInterrupt = 4,
        NBInterruptOnCompletion = 8,
        NBFifoError = 0x10,
    };

    enum NABMTransferControlFlags {
        // 0 - pause transfer, 1 - transfer sound data
        NBTransferDMAControl = 1,
        // 0 - remove reset condition
        // 1 - reset
        // cleared when reset complete
        NBTransferReset = 2,
        //already defined NBLastBufferEntryInterrupt = 4,
        //already defined NBInterruptOnCompletion = 8,
        NBFIFOErrorInterrupt = 0x10,
    };

    enum NABMBDLFlags {
        BDInterruptOnCompletion = 1 << 15,
        // If set, AC97 stops playing audio after this entry
        BDLastEntry = 1 << 14,
    };

    struct BufferDescriptor {
        uint32_t address;
        // Must be an even number of sample (2 channels)
        uint16_t sampleCount;
        uint16_t flags;
    } __attribute__((packed));

    lock_t m_lock = 0;

    uint16_t m_ioPort;
    uint16_t m_nabmPort;

    // Native Audio Bus MAster
    uintptr_t m_busMasterBaseAddress;

    PCMOutput* m_pcm;
    SoundEncoding m_pcmEncoding;
    int m_pcmNumChannels = 2;
    uint16_t m_pcmSampleSize = 2;

    uintptr_t bufferDescriptorListPhys;
    BufferDescriptor* bufferDescriptorList;

    // For now use two buffer entries
    uintptr_t sampleBuffersPhys[32];
    uint16_t* sampleBuffers[32];
    // Amount of samples per channel in each buffer
    int m_samplesPerBuffer;
};

}