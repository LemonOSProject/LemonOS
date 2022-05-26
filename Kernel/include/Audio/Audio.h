#pragma once

#include <ABI/Audio.h>

#include <Device.h>

namespace Audio {

struct PCMOutput {
    class AudioController* c;
    void* output;
};

class AudioController : public Device {
public:
    AudioController();
    virtual ~AudioController();

    virtual int SetMasterVolume(int percentage) = 0;
    virtual int GetMasterVolume() const = 0;

    virtual int OutputSetVolume(void* output, int percentage) = 0;
    virtual int OutputGetVolume(void* output) const = 0;
    virtual SoundEncoding OutputGetEncoding(void* output) const = 0;
    // The audio system will expect interleaved samples,
    // e.g. for dual channel audio the first sample will be for left channel,
    // second sample for right, third for left, etc.
    virtual int OutputNumberOfChannels(void* output) const = 0;
    virtual int OutputSampleRate(void* output) const = 0;
    virtual int OutputSetNumberOfChannels(int channels) = 0;

    virtual int WriteSamples(void* output, uint8_t* buffer, size_t size, bool async) = 0;
private:

};

void InitializeSystem();

void RegisterPCMOut(PCMOutput* out);
Error UnregisterPCMOut(PCMOutput* out);

}
