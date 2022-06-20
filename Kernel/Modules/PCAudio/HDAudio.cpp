#include "HDAudio.h"

#include <IDT.h>
#include <Math.h>
#include <Memory.h>
#include <Timer.h>

namespace Audio {

int IntelHDAudioController::SetMasterVolume(int percentage) { return 0; }

int IntelHDAudioController::GetMasterVolume() const { return 0; }

int IntelHDAudioController::OutputSetVolume(void* output, int percentage) { return 0; }

int IntelHDAudioController::OutputGetVolume(void* output) const { return 0; }

SoundEncoding IntelHDAudioController::OutputGetEncoding(void* output) const {
    if(m_outputs.get_length()) {
        return m_outputs[0]->sampleFmt;
    }

    return PCMS16LE;
}

int IntelHDAudioController::OutputNumberOfChannels(void* output) const {
    if(m_outputs.get_length()) {
        return m_outputs[0]->channels;
    }

    return -ENODEV;
}

int IntelHDAudioController::OutputSampleRate(void* output) const {
    if(m_outputs.get_length()) {
        return m_outputs[0]->sampleRate;
    }

    return -ENODEV;
}

int IntelHDAudioController::OutputSetNumberOfChannels(int channels) { return -ENOSYS; }

int IntelHDAudioController::WriteSamples(void*, uint8_t* buffer, size_t size, bool) {
    return -ENOSYS;
}

void HDAIRQ(void* c, RegisterContext*) { ((IntelHDAudioController*)c)->OnInterrupt(); }

void IntelHDAudioController::OnInterrupt() {
    if (m_cRegs->rirbStatus & 0x4) {
        Log::Warning("[HDAudio] RIRB Overrun");
    }

    // Clear overrun and interrupt flags
    m_cRegs->rirbStatus |= 4 | 1;
}

IntelHDAudioController::IntelHDAudioController(const PCIInfo& info) : PCIDevice(info) {
    SetDeviceName("Intel HD Audio Controller");

    uintptr_t bar = GetBaseAddressRegister(0);
    assert(!BarIsIOPort(0));

    m_cRegs = reinterpret_cast<ControllerRegs*>(Memory::KernelAllocate4KPages(4));

    EnableBusMastering();
    EnableInterrupts();
    EnableMemorySpace();

    uint8_t v = AllocateVector(PCIVectorLegacy);
    assert(v != 0xff);

    IDT::RegisterInterruptHandler(v, HDAIRQ, this);

    Memory::KernelMapVirtualMemory4K(bar, (uintptr_t)m_cRegs, 4, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED);

    long timer = 200;
    m_cRegs->globalControl |= 1; // Set CRST bit to leave reset mode
    while (!(m_cRegs->globalControl & 0x1) && timer--)
        Timer::Wait(1);

    if (timer <= 0) {
        Log::Error("[HDAudio] Timed out waiting for controller to exit reset mode");
        return;
    }

    KernelAllocateMappedBlock<uint32_t, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED>(&m_corbPhys, &m_corb);
    KernelAllocateMappedBlock<uint64_t, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLED>(&m_rirbPhys, &m_rirb);

    // Disable interrupts
    m_cRegs->intControl = 0;

    // Stop CORB, RIRB
    m_cRegs->corbControl &= ~2U;
    m_cRegs->rirbControl &= ~2U;

    timer = 200;
    while (((m_cRegs->corbControl | m_cRegs->rirbControl) & 0x2) && timer--)
        Timer::Wait(1);

    if (timer <= 0) {
        Log::Error("[HDAudio] Timed out waiting for controller");
        return;
    }

    Log::Info("[HDAudio] Initializing Intel HD Audio Controller %x:%x (Base: %x, Virtual Base: %x, Global Control: %x, "
              "Interrupt: %x)",
              VendorID(), DeviceID(), bar, m_cRegs, m_cRegs->globalControl, v);

    bool supports64Bit = HDA_GCAP_64BIT(m_cRegs->globalCap);
    int outputStreams = HDA_GCAP_OUTPUT_STREAMS(m_cRegs->globalCap);
    int inputStreams = HDA_GCAP_INPUT_STREAMS(m_cRegs->globalCap);
    int biStreams = HDA_GCAP_BIDIRECTIONAL_STREAMS(m_cRegs->globalCap);

    Log::Info("[HDAudio] 64-bit addressing: %Y Output streams: %d Input streams: %d Bidirectional streams: %d",
              supports64Bit, outputStreams, inputStreams, biStreams);

    m_numInputStreams = inputStreams;
    m_numOutputStreams = outputStreams;
    m_numBidStreams = biStreams;

    m_cRegs->dmaPositionBuffer = 0;

    if (m_cRegs->corbSize & HDA_SUPPORTED_CORB_SIZE_256) {
        // Check if any other sizes are supported
        // if so, we need to set corb size
        if ((m_cRegs->corbSize & HDA_SUPPORTED_CORB_SIZE_MASK) != HDA_SUPPORTED_CORB_SIZE_256) {
            m_cRegs->corbSize = (m_cRegs->corbSize & ~0x3) | HDA_CORB_SIZE_256;
        }

        m_corbEntries = 256;
    } else if (m_cRegs->corbSize & HDA_SUPPORTED_CORB_SIZE_16) {
        if ((m_cRegs->corbSize & HDA_SUPPORTED_CORB_SIZE_MASK) != HDA_SUPPORTED_CORB_SIZE_16) {
            m_cRegs->corbSize = (m_cRegs->corbSize & ~0x3) | HDA_CORB_SIZE_16;
        }

        m_corbEntries = 16;
    } else if (m_cRegs->corbSize & HDA_SUPPORTED_CORB_SIZE_2) {
        // Only available size, no need to set
        m_corbEntries = 2;
    }

    assert(m_cRegs->rirbSize & HDA_SUPPORTED_CORB_SIZE_256);
    // m_cRegs->rirbSize = (m_cRegs->rirbSize & ~0x3) | HDA_RIRB_SIZE_256;
    m_rirbEntries = 256;

    m_cRegs->corbBase = m_corbPhys;
    m_cRegs->rirbBase = m_rirbPhys;

    m_cRegs->corbWriteP &= ~HDA_CORB_WRITEP_MASK;

    // Reset CORB and RIRB pointers
    m_cRegs->corbReadP |= (1 << 15);
    m_cRegs->rirbWriteP |= (1 << 15);

    // Wait for the CORB and RIRB pointers to be reset
    timer = 200;
    while (((m_cRegs->corbReadP | m_cRegs->corbWriteP) & (1 << 15)) & timer--) {
        Timer::Wait(1);
    }

    if (timer <= 0) {
        Log::Error("[HDAudio] Timed out waiting for controller");
        return;
    }

    // Disable wake interrupts
    m_cRegs->wakeEnable = (m_cRegs->wakeEnable & ~0x7fU);

    // Enable interrupts and unsolicited repsonses
    m_cRegs->globalControl |= (1 << 8);
    m_cRegs->intControl = (1U << 31) | (1U << 30);

    m_cRegs->rIntCount = 1;

    // Start CORB and RIRB
    // Also enable interrupts
    m_cRegs->corbControl |= 3;
    m_cRegs->rirbControl |= 3;

    // Must wait at least 521 us
    // before assuming codecs have been registered
    Timer::Wait(10);

    uint32_t status = m_cRegs->wakeStatus;
    Log::Info("[HDAudio] Wake status: %d", status);
    if (!status) {
        Log::Error("[HDAudio] No codecs are awake!");
        return;
    }

    int i = HDA_MAX_CODECS;
    while (i--) {
        if (status & (1 << i)) {
            DetectCodec(i);
        }
    }

    m_pcmOut.c = this;
    m_pcmOut.output = nullptr;

    Audio::RegisterPCMOut(&m_pcmOut);
}

void IntelHDAudioController::DetectCodec(uint32_t i) {
    assert(i < HDA_MAX_CODECS);

    HDACodec& codec = m_codecs[i];
    codec.address = i;

    uint32_t vID = GetCodecParameter(i, 0, WidgetParameterVendorID);
    if (vID == 0xffffffff) {
        return;
    }

    uint32_t rID = GetCodecParameter(i, 0, WidgetParameterRevisionID);
    uint32_t subNodes = GetCodecParameter(i, 0, WidgetParameterSubNodeCount);

    codec.firstNode = subNodes >> 16;
    codec.subnodes = subNodes & 0xff;

    Log::Info("[HDAudio] Codec %d, VendorID: %x, RevisionID: %x, Sub Nodes: %d", i, vID, rID, subNodes & 0xff);

    for (uint32_t n = codec.firstNode; n < codec.firstNode + codec.subnodes; n++) {
        uint32_t f = GetCodecParameter(i, n, WidgetParameterFunctionGroupType);
        uint8_t ftype = f & 0xff;
        if (ftype != WidgetFunctionGroupAudio) {
            codec.widgets.add_back({n, 0, 0});
            continue;
        }

        uint32_t cap = GetCodecParameter(i, n, WidgetParameterAudioWidgetCapabilities);
        uint8_t type = HDA_W_AUDIO_TYPE(cap);
        if (type == HDA_W_AUDIO_OUTPUT) {
            Log::Info("[HDAudio] Found audio output at %d:%d", i, n);
            AddCodecOutput(i, n);
        }

        codec.widgets.add_back({n, type, cap});
    }
}

uint32_t IntelHDAudioController::GetCodecParameter(uint32_t codec, uint32_t node, uint32_t parameter) {
    assert(codec < HDA_MAX_CODECS);

    // 4-bit verb ID, 12-bit payload for Codec parameters
    uint64_t response;
    int result = SendVerb(MakeVerb((CodecGetParameter << 8) | parameter, node, codec), &response);
    if (result) {
        return 0xffffffff;
    }

    uint32_t value = response & 0xffffffff;

    // Check the codec of the response
    uint32_t info = response >> 32;
    Log::Info("Codec %d, inf %d, v: %d", codec, info, value);
    assert((info & 0xf) == codec);

    return value;
}

void IntelHDAudioController::AddCodecOutput(int codec, int node) {
    HDAOutput* out = new HDAOutput;
    out->codec = codec;
    out->node = node;

    out->sampleFmt = SoundEncoding::PCMS16LE;
    out->sampleSize = 2;
    out->sampleRate = STREAM_SAMPLE_RATE_BASE_0;
    out->channels = 2;

    StreamFormat fmt;
    fmt.value = 0;
    // 24-bit audio samples (011b)
    fmt.bits = 3;

    // 48000hz
    fmt.div = 0;
    fmt.mult = 0;
    fmt.base = 0;

    out->stream = CreateStream(codec, STREAM_ID_PCMOUT, fmt, StreamType::Output);

    uint64_t response;
    int result =
        SendVerb(MakeVerb(CodecSetConverterStreamChannel << 8 | HDA_CODEC_SET_STREAM_CHAN_PAYLOAD(STREAM_ID_PCMOUT, 0),
                          node, codec),
                 &response);
    assert(!result);

    m_outputs.add_back(out);
}

FancyRefPtr<HDAStream> IntelHDAudioController::CreateStream(int codec, int num, StreamFormat fmt, StreamType type) {
    HDAStream* stream = new HDAStream;
    if (type == StreamType::Output) {
        stream->descriptor = codec;
    } else if (type == StreamType::Input) {
        stream->descriptor = m_numOutputStreams + codec;
    } else if (type == StreamType::Bidirectional) {
        stream->descriptor = m_numOutputStreams + m_numInputStreams + codec;
    }

    stream->streamNumber = num;

    StreamDescriptor* desc = &m_cRegs->streams[stream->descriptor];

    stream->bdlEntries = PAGE_SIZE_4K / sizeof(HDABufferDescriptorEntry);
    KernelAllocateMappedBlock(&stream->bdlPhys, &stream->bdl);

    for (int i = 0; i < stream->bdlEntries; i++) {
        stream->bdl[i].length = PAGE_SIZE_4K;
        stream->bdl[i].ioc = 0;

        void* buffer;
        KernelAllocateMappedBlock(&stream->bdl[i].address, &buffer);

        stream->buffers.add_back(buffer);
    }

    desc->bufferDescListPtr = stream->bdlPhys;
    desc->bufferDescListPtrHigh = stream->bdlPhys >> 32;

    // Exit stream reset
    desc->control &= ~HDA_STREAM_CTL_RST;

    int timer = 200;
    while ((desc->control & HDA_STREAM_CTL_RST) && timer--) {
        Timer::Wait(2);
    }
    // Make sure we didnt time out waiting for stream to exit reset
    assert(timer > 0);

    // Clear run, stripe control, stream number, etc.
    desc->control = desc->control &
                    ~(HDA_STREAM_CTL_RST | HDA_STREAM_CTL_RUN | HDA_STREAM_CTL_STREAM_NUM_MASK |
                      HDA_STREAM_CTL_STRIPE_MASK | HDA_STREAM_CTL_TRAFFIC_PRIORITY | HDA_STREAM_CTL_BIDIRECTIONAL_DIR);
    // Enable intrrupts
    desc->control |= HDA_STREAM_CTL_ICOE | HDA_STREAM_CTL_FEIE | HDA_STREAM_CTL_DEIE;
    // Set stream number
    desc->control |= HDA_STREAM_CTL_STREAM_NUM(num);

    // Clear status bits
    desc->status |= HDA_STREAM_STS_BCIS | HDA_STREAM_STS_FIFO_ERROR | HDA_STREAM_STS_DESC_ERROR;

    desc->cyclicBufferLength = stream->bdlEntries;
    desc->lastValidIndex = (desc->lastValidIndex & ~0xff) | stream->bdlEntries;
    // Save bit 7 (reserved)
    desc->format = (desc->format & (1 << 7)) | fmt.value;

    return stream;
}

int IntelHDAudioController::SendVerb(uint32_t verb, uint64_t* response) {
    uint16_t writePointer = (m_cRegs->corbWriteP + 1) % m_corbEntries;
    uint16_t readPointer = m_cRegs->rirbWriteP;

    assert(writePointer != m_cRegs->corbReadP);

    m_corb[writePointer] = verb;
    m_cRegs->corbWriteP = writePointer;

    // Wait 200ms for a response
    int timer = 100;
    while (m_cRegs->rirbWriteP == readPointer && timer--) {
        Timer::Wait(2);
    }

    if (timer <= 0) {
        Log::Warning("[HDAudio] Timed out waiting for verb response");
        return 1;
    }

    *response = m_rirb[(readPointer + 1) % m_rirbEntries];
    return 0;
}

} // namespace Audio