#pragma once

// Intel HD Audio Driver for Lemon OS

#include "Audio.h"

#include <Device.h>
#include <List.h>
#include <Lock.h>
#include <PCI.h>
#include <RefPtr.h>
#include <Paging.h>

#define HDA_GCAP_OUTPUT_STREAMS(x) ((x >> 12) & 0xf)
#define HDA_GCAP_INPUT_STREAMS(x) ((x >> 8) & 0xf)
#define HDA_GCAP_BIDIRECTIONAL_STREAMS(x) ((x >> 3) & 0xf)
#define HDA_GCAP_64BIT(x) (x & 0x1)

// In the corbsize register,
// the top 4 bits indicate the supported size
// 2 entries (8 bytes), 16 entries, 256 entries
#define HDA_SUPPORTED_CORB_SIZE_MASK (7U << 4)
#define HDA_SUPPORTED_CORB_SIZE_2 (1U << 4)
#define HDA_SUPPORTED_CORB_SIZE_16 (2U << 4)
#define HDA_SUPPORTED_CORB_SIZE_256 (4U << 4)

#define HDA_RB_SIZE_MASK 0x3

#define HDA_CORB_SIZE_2 0
#define HDA_CORB_SIZE_16 1U
#define HDA_CORB_SIZE_256 2U

#define HDA_RIRB_SIZE_2 0
#define HDA_RIRB_SIZE_16 1U
#define HDA_RIRB_SIZE_256 2U

#define HDA_CORB_WRITEP_MASK 0xffU

// Stream reset
#define HDA_STREAM_CTL_RST 0x1
// Stream run
#define HDA_STREAM_CTL_RUN 0x2
// Interrupt on completion enable
#define HDA_STREAM_CTL_ICOE 0x4
// FIFO Error Interrupt
#define HDA_STREAM_CTL_FEIE 0x8
// Descriptor error interuupt
#define HDA_STREAM_CTL_DEIE 0x10

#define HDA_STREAM_CTL_STRIPE_MASK (3 << 16)
#define HDA_STREAM_CTL_TRAFFIC_PRIORITY (1 << 18)
// Bid direction control
#define HDA_STREAM_CTL_BIDIRECTIONAL_DIR (1 << 19)
// 'tag' associated with stream data
#define HDA_STREAM_CTL_STREAM_NUM_MASK (0xf << 20)
#define HDA_STREAM_CTL_STREAM_NUM(x) ((x & 0xf) << 20)

#define HDA_STREAM_FMT_CHAN_MASK (0xf)
// BIts per sample
#define HDA_STREAM_FMT_BITS_MASK (0x7 << 4)
#define HDA_STREAM_FMT_DIV_MASK (0x7 << 8)
#define HDA_STREAM_FMT_MULT_MASK (0x7 << 11)

// Buffer completion interrupt status
#define HDA_STREAM_STS_BCIS 0x2
#define HDA_STREAM_STS_FIFO_ERROR 0x4
#define HDA_STREAM_STS_DESC_ERROR 0x8
#define HDA_STREAM_STS_FIFO_READY 0x10

#define HDA_MAX_CODECS 15

// Payload for set stream, channel command
#define HDA_CODEC_SET_STREAM_CHAN_PAYLOAD(stream, chan) (((stream & 0xf) << 4) | (chan & 0xf))

// Audio widget capabilities
#define HDA_W_AUDIO_CHAN_COUNT 0x1
#define HDA_W_AUDIO_AMP_IN 0x2
#define HDA_W_AUDIO_AMP_OUT 0x4
#define HDA_W_AUDIO_FORMAT_OVERRIDE 0x8
#define HDA_W_AUDIO_STRIPE 0x10

#define HDA_W_AUDIO_TYPE(x) ((x >> 20) & 0xf)
#define HDA_W_AUDIO_OUTPUT 0
#define HDA_W_AUDIO_INPUT 1
#define HDA_W_AUDIO_MIXER 2
#define HDA_W_AUDIO_SELECTOR 3
#define HDA_W_AUDIO_PIN_COMPLEX 4
#define HDA_W_AUDIO_POWER_WIDGET 5
#define HDA_W_AUDIO_VOLUME_KNOB 6
#define HDA_W_AUDIO_BEEP_GENERATOR 7
#define HDA_W_AUDIO_VENDOR_DEFINED 0xf

// Default stream number for output
#define STREAM_ID_PCMOUT 1

// If the base of stream format is '1' then the base format
// is 44100Hz
#define STREAM_SAMPLE_RATE_BASE_0 48000
#define STREAM_SAMPLE_RATE_BASE_1 44100

namespace Audio {
struct HDAWidget {
    // Relevant node
    uint32_t node;
    // Function Group Type
    uint8_t type;
    // Audio capabilities
    uint32_t audioCap;
};

struct HDACodec {
    uint32_t address;

    uint32_t firstNode;
    uint32_t subnodes;

    List<HDAWidget> widgets;
};

struct HDABufferDescriptorEntry {
    uint64_t address;
    uint32_t length;
    // Interrupt on completion
    uint32_t ioc : 1;
    uint32_t reserved : 31;
} __attribute__((packed));
static_assert(!(PAGE_SIZE_4K % sizeof(HDABufferDescriptorEntry)));

struct HDAStream {
    int descriptor;
    int streamNumber;

    uintptr_t bdlPhys;
    uint32_t bdlEntries;
    HDABufferDescriptorEntry* bdl;

    Vector<void*> buffers;
};

struct HDAOutput {
    int codec;
    int node;

    FancyRefPtr<HDAStream> stream;

    int sampleSize; // Padded sample size in bytes
    int channels;
    int sampleRate;
    SoundEncoding sampleFmt;
};

class IntelHDAudioController : public AudioController, PCIDevice {
    enum CodecVerb {
        CodecGetParameter = 0xf00,
        CodecGetConnectionListEntry = 0xf02,
        CodecGetProcessingState = 0xf03,
        CodecSetProcessingState = 0x703,
        CodecGetCoefficientIndex = 0xd,
        CodecSetCoefficientIndex = 0x5,
        CodecGetCoefficient = 0xc,
        CodecSetCoefficient = 0x4,
        // Amplifier gain/mute
        CodecGetAmpGainMute = 0xb,
        CodecSetAmpGainMute = 0x3,
        CodecGetConverterFormat = 0xa,
        CodecSetConverterFormat = 0x2,
        CodecGetPowerState = 0xf05,
        CodecSetPowerState = 0x705,
        CodecGetConverterStreamChannel = 0xf06,
        CodecSetConverterStreamChannel = 0x706,
        CodecGetBeepGeneration = 0xf0a,
        CodecSetBeepGeneration = 0x70a,
        CodecGetVolumeKnob = 0xf0f,
        CodecSetVolumeKnob = 0x70f,
        CodecGetConfigurationDefault = 0xf1c,
        CodecSetConfigurationDefault1 = 0x71c,
        CodecSetConfigurationDefault2 = 0x71d,
        CodecSetConfigurationDefault3 = 0x71e,
        CodecSetConfigurationDefault4 = 0x71f,
        CodecGetStripeControl = 0xf24,
        CodecSetStripeControl = 0x724,
        CodecFunctionReset = 0x7ff,
    };

    enum CodecParameter {
        WidgetParameterVendorID = 0,
        WidgetParameterRevisionID = 0x2,
        WidgetParameterSubNodeCount = 0x4,
        WidgetParameterFunctionGroupType = 0x5,
        WidgetParameterFunctionGroupCapabilities = 0x8,
        WidgetParameterAudioWidgetCapabilities = 0x9,
        // Returns max bit depth and sample rate
        // upper 16 bits - depth
        // lower 16 - rate
        WidgetParameterPCMSizeRates = 0xa,
        // 0 - PCM
        // 1 - float32
        // 2 - ac3
        WidgetParameterSupportedStreamFormats = 0xb,
        WidgetParameterPinCapabilities = 0xc,
        WidgetParameterAmpInputCapabilities = 0xd,
        WidgetParameterAmpOutputCapabilities = 0x12,
        WidgetParameterConnectionListLength = 0xe,
        WidgetParameterSupportedPowerStates = 0xf,
        WidgetParameterProcessingCapabilities = 0x10,
        WidgetParameterGPIOCount = 0x11,
        WidgetParameterVolumeKnobCapabilities = 0x13,
    };

    enum FunctionGroup {
        WidgetFunctionGroupAudio = 0x1,
    };

    union StreamFormat {
        uint16_t value;
        struct {
            // 0b0000 - 1
            // 0b0001 - 2
            // ...
            // 0b1111 - 16
            uint16_t numOfChannels : 4;
            // Bits per sample
            // 000 - 8
            // 001 - 16
            // 010 - 20
            // 011 - 24
            // 100 - 32
            uint16_t bits : 3;
            uint16_t resv : 1;
            // Sample base rate divisor
            // 000 - divide by 1 (48, 44.1 kHz)
            // 001 - divide by 2 (24, 22.05kHz)
            // ...
            // 111 - divide by 8
            uint16_t div : 3;
            // Sample base rate multiple
            uint16_t mult : 3;
            // Sample Base Rate
            // 0 - 48kHz
            // 1 - 44.1kHz
            uint16_t base : 1;
            // Stream type
            // 0 - PCM (values in structure are valid)
            // 1 - not PCM
            uint16_t type : 1;
        } __attribute__((packed));
    } __attribute__((packed));

    struct StreamDescriptor {
        uint32_t control : 24; // Stream Descriptor Control
        uint8_t status;
        uint32_t linkPosInCurrentBuffer; // ISD0 Link Position in Current Buffer
        uint32_t cyclicBufferLength;
        uint16_t lastValidIndex;
        uint8_t reserved10[2];
        uint16_t streamFIFOSize;
        uint16_t format;
        uint8_t reserved11[4];
        uint32_t bufferDescListPtr;
        uint32_t bufferDescListPtrHigh;
    } __attribute__((packed));

    struct ControllerRegs {
        uint16_t globalCap; // Global Capabilities
        uint8_t minorVersion;
        uint8_t majorVersion;
        uint16_t outputPayloadCap; // Output Payload Capability
        uint16_t inputPayloadCap;  // Input Payload Capability
        uint32_t globalControl;    // Global Control
        uint16_t wakeEnable;       // Wake Enable
        uint16_t wakeStatus;       // Wake Status
        uint16_t globalStatus;     // Global Status
        uint8_t reserved[6];
        uint16_t outputStreamPayloadCap; // Output Stream Payload Capability
        uint16_t inputStreamPayloadCap;  // Input Stream Payload Capability
        uint8_t reserved2[4];
        uint32_t intControl; // Interrupt Control
        uint32_t intStatus;  // Interrupt Status
        uint8_t reserved3[8];
        uint32_t wallClock; // Wall Clock Counter
        uint8_t reserved4[4];
        uint32_t streamSync; // Stream Synchronizatoin
        uint8_t reserved5[4];
        uint32_t corbBase; // CORB Base Address
        uint32_t corbBaseHigh;
        uint16_t corbWriteP; // CORB Write Pointer
        uint16_t corbReadP;  // CORB Read Pointer
        uint8_t corbControl; // CORB Control
        uint8_t corbStatus;  // CORB Status
        uint8_t corbSize;    // CORB Size
        uint8_t reserved6;
        uint32_t rirbBase; // RIRB Base
        uint32_t rirbBaseHigh;
        uint16_t rirbWriteP;
        uint16_t rIntCount; // Response Interrupt Count
        uint8_t rirbControl;
        uint8_t rirbStatus;
        uint8_t rirbSize;
        uint8_t reserved7;
        uint32_t immediateCmdOutputInterface; // Immediate Command Output Interface
        uint32_t immediateCmdInputInterface;  // Immediate Command Output Interface
        uint16_t immediateCmdStatus;
        uint8_t reserved8[6];
        uint32_t dmaPositionBuffer;
        uint32_t dmaPositionBufferHigh;
        uint8_t reserved9[8];
        StreamDescriptor streams[];
    } __attribute__((packed));

	enum class StreamType {
		Output,
		Input,
		Bidirectional,
	};

public:
    IntelHDAudioController(const PCIInfo& info);

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

    void OnInterrupt();

private:
    void DetectCodec(uint32_t index);
    uint32_t GetCodecParameter(uint32_t codec, uint32_t node, uint32_t parameter);

    void AddCodecOutput(int codec, int node);
	FancyRefPtr<HDAStream> CreateStream(int codec, int num, StreamFormat fmt, StreamType type);

    // Verb:
    // 31:28 - codec address
    // 27:20 - node ID
    // 19:0 - verb payload
    inline constexpr uint32_t MakeVerb(uint32_t payload, uint32_t nodeID, uint32_t codecAddress) {
        return (payload & 0xfffff) | ((nodeID & 0xff) << 20) | ((codecAddress & 0xf) << 28);
    }

    int SendVerb(uint32_t verb, uint64_t* response);

    // Command output ring buffer
    uintptr_t m_corbPhys;
    uint32_t m_corbEntries;
    uint32_t* m_corb;

    // Response input ring buffer
    uintptr_t m_rirbPhys;
    uint32_t m_rirbEntries;
    uint64_t* m_rirb;

    HDACodec m_codecs[HDA_MAX_CODECS];
    Vector<HDAOutput*> m_outputs;

    ControllerRegs* m_cRegs;

    int m_numInputStreams;
    int m_numOutputStreams;
    int m_numBidStreams;

    PCMOutput m_pcmOut;
};
} // namespace Audio