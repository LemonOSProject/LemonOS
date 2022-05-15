#pragma once

enum SoundEncoding {
    PCMS16LE, // PCM signed 16-bit little endian
    PCMS20LE,
};

enum AudioMixerIoCtl {
    IoCtlMixerSetMasterVolume = 0x1000,
    IoCtlMixerGetMasterVolume = 0x1001,
};

enum AudioOutputIoCtl {
    IoCtlOutputSetVolume = 0x1000,
    IoCtlOutputGetVolume = 0x1001,
    IoCtlOutputGetEncoding = 0x1002,
    IoCtlOutputGetSampleRate = 0x1003,
    IoCtlOutputSetNumberOfChannels = 0x1004,
    IoCtlOutputGetNumberOfChannels = 0x1005,
};

static const char* const encodingNames[] = {
    "PCM signed 16-bit little endian",
};
