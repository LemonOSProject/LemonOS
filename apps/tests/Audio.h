#pragma once

#include "Test.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include <sys/ioctl.h>

#include <lemon/system/abi/Audio.h>

#define AUDIO_NOTE_E4 329.63
#define AUDIO_NOTE_FS4 369.99
#define AUDIO_NOTE_GS4 415.30

int RunAudio() {
    int fd = open("/dev/snd/pcm", O_WRONLY);
    if(fd == -1) {
        perror("/dev/snd/pcm:");
        return 1;
    }
    
    int sampleRate = ioctl(fd, IoCtlOutputGetSampleRate);
    
    printf("PCM sample rate: %d\n", sampleRate);
    printf("Volume: %d%% Channels: %d\n", ioctl(fd, IoCtlOutputGetVolume), ioctl(fd, IoCtlOutputGetNumberOfChannels));

    if(sampleRate == -1) {
        return 2; // Error getting sample rate
    }

    uint16_t samples[48000*2];

    auto squareWaveFunction = [&](int i) -> uint16_t {
        // Attempt to write an 'A4' note (440hz)
        const double notes[8] = {1 / AUDIO_NOTE_E4, 1 / AUDIO_NOTE_FS4, 1 / AUDIO_NOTE_GS4, 0, 1 / AUDIO_NOTE_E4, 0, 1 / AUDIO_NOTE_FS4, 1 / AUDIO_NOTE_GS4};
        // Change notes every eighth of a second
        double halfWavelength = (sampleRate * notes[(i / (sampleRate / 8)) % 3]) / 2;
        
        // Either 0 or 1
        int amplitude = (int)(i / halfWavelength) % 2;
        // Center is at 0x8000
        return 0x4000 + amplitude * 0x8000;
    };

    for (int i = 0; i < 48000; i++) {
        samples[i * 2] = squareWaveFunction(i);
        samples[i * 2 + 1] = samples[i * 2];
    }

    write(fd, samples, 48000*2*2);
    return 0;
}

static Test audioTest = {
    .func = RunAudio,
    .prettyName = "Audio Driver Test"
};
