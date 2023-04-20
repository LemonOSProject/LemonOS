#include <Audio/Audio.h>

namespace Audio {

lock_t pcmOutputsLock = 0;
List<PCMOutput*> pcmOutputs;
PCMOutput* currentOutput;

class MixerDevice
    : public FsNode {
public:
    MixerDevice () {
        type = FileType::CharDevice;
    }

    ErrorOr<int> ioctl(uint64_t cmd, uint64_t arg) override {
        switch(cmd) {
        default:
            return Error{EINVAL};
        }

        return Error{EINVAL};
    }
};

class PCMOutputDevice
    : public FsNode {
public:
    PCMOutputDevice() {
        type = FileType::CharDevice;
    }

    ErrorOr<int> ioctl(uint64_t cmd, uint64_t arg) override {
        ScopedSpinLock lockOutputs(pcmOutputsLock);
        if(!currentOutput) {
            Log::Warning("no audio output!", size);
            return 0;
        }

        AudioController* c = currentOutput->c;
        void* out = currentOutput->output;
        switch (cmd)
        {
        case IoCtlOutputSetVolume:
            return c->OutputSetVolume(out, (int)arg);
        case IoCtlOutputGetVolume:
            return c->OutputGetVolume(out);
        case IoCtlOutputGetEncoding:
            return c->OutputGetEncoding(out);
        case IoCtlOutputGetSampleRate:
            return c->OutputSampleRate(out);
        case IoCtlOutputGetNumberOfChannels:
            return c->OutputNumberOfChannels(out);
        case IoCtlOutputSetAsync:
            m_async = (bool)arg;
            return 0;
        case IoCtlOutputSetNumberOfChannels:
        default:
            return -EINVAL;
        }
    }

    ErrorOr<ssize_t> write(size_t off, size_t size, UIOBuffer* buffer) override {
        ScopedSpinLock lockOutputs(pcmOutputsLock);
        if(!currentOutput) {
            // If no output treat as dummy device
            Log::Warning("no audio output!", size);
            return size;
        }

        AudioController* c = currentOutput->c;
        return c->WriteSamples(currentOutput->output, buffer, size, m_async);
    }
private:
    bool m_async = false;
};

class SoundFS
    : public Device {
public:
    SoundFS()
        : Device("snd", DeviceTypeUnknown) {
        type = FileType::Directory;
    }

    ErrorOr<int> read_dir(DirectoryEntry* dirent, uint32_t index) override {
        switch(index) {
        case 0:
            *dirent = DirectoryEntry(this, ".");
            return 1;
        case 1:
            *dirent = DirectoryEntry(DeviceManager::GetDevFS(), "..");
            return 1;
        case 2:
            *dirent = DirectoryEntry(&mixer, "mixer");
            return 1;
        case 3:
            *dirent = DirectoryEntry(&pcm, "pcm");
            return 1;
        default:
            return 0;
        }
    }

    ErrorOr<FsNode*> find_dir(const char* name) override;

    PCMOutputDevice pcm = PCMOutputDevice();
    MixerDevice mixer = MixerDevice();
};

ErrorOr<FsNode*> SoundFS::find_dir(const char* name) {
    if(strcmp(name, "mixer") == 0) {
        return &mixer;
    }

    if(strcmp(name, "pcm") == 0) {
        return &pcm;
    }

    return Error{ENOENT};
}

AudioController::AudioController()
    : Device(DeviceTypeAudioController) {

}

AudioController::~AudioController() {

}

SoundFS* sfs;
void InitializeSystem() {
    sfs = new SoundFS();
}

void RegisterPCMOut(PCMOutput* out) {
    ScopedSpinLock lockOutputs(pcmOutputsLock);

    pcmOutputs.add_back(out);
    currentOutput = out;
}

Error UnregisterPCMOut(PCMOutput* out) {
    ScopedSpinLock lockOutputs(pcmOutputsLock);
    for(auto it = pcmOutputs.begin(); it != pcmOutputs.end(); it++) {
        if(*it == out) {
            pcmOutputs.remove(it);
            
            if(pcmOutputs.get_length()) {
                currentOutput = pcmOutputs.get_front();
            } else {
                currentOutput = nullptr;
            }

            return ERROR_NONE;
        }
    }

    return Error{ENOENT};
}

} // namespace Audio
