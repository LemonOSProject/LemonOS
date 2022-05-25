#pragma once

#include <thread>
#include <mutex>

#include "AudioTrack.h"

#define AUDIOCONTEXT_NUM_SAMPLE_BUFFERS 32

class AudioContext {
    friend void DecodeAudio(AudioContext*);
    friend void PlayAudio(AudioContext*);
public:
    struct SampleBuffer {
        uint8_t* data;
        int samples;

        // Timestamp in seconds of last frame in buffer
        float timestamp;
    };

    AudioContext();

    inline bool IsAudioPlaying() const { return m_isDecoderRunning; }

    inline float PlaybackProgress() const;

    void PlaybackStart();
    void PlaybackPause();
    void PlaybackStop();

    int PlayTrack(std::string filepath);
    int LoadTrack(std::string filepath, TrackInfo& info);

    SampleBuffer sampleBuffers[AUDIOCONTEXT_NUM_SAMPLE_BUFFERS];
    ssize_t samplesPerBuffer;
    // Ring buffer of SampleBuffers
    // Index of the last valid sample buffer
    int lastSampleBuffer;
    // The currently processed sample buffer
    int currentSampleBuffer;
    std::atomic<int> numValidBuffers;

private:
    // File descriptor for pcm output
    int m_pcmOut;
    int m_pcmSampleRate;
    int m_pcmChannels;
    int m_pcmBitDepth;

    // Thread which writes PCM samples to the audio driver,
    // reduces audio lag and prevents blocking the main thread
    // so the decoder and GUI can run
    std::thread m_playbackThread;

    // Decoder runs parallel to playback and GUI threads
    std::thread m_decoderThread;
    // Held by the decoderThread whilst it is running
    std::mutex m_decoderLock;
    bool m_isDecoderRunning = false;

    struct AVFormatContext* m_avfmt = nullptr;
    struct AVCodecContext* m_avcodec = nullptr;
    
    struct AVStream* m_currentStream = nullptr;
};
