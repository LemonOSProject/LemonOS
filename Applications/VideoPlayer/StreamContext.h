#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#define StreamContext_NUM_SAMPLE_BUFFERS 16

class StreamContext {
    friend void PlayAudio(StreamContext*);

public:
    struct SampleBuffer {
        uint8_t* data;
        int samples;

        // Timestamp in seconds of last frame in buffer
        float timestamp;
    };

    StreamContext();
    ~StreamContext();

    void SetDisplaySurface(struct Surface* surf);

    inline bool HasLoadedAudio() const { return m_isDecoderRunning; }
    inline bool IsAudioPlaying() const { return m_shouldPlayAudio; }
    inline bool ShouldPlayNextTrack() const { return m_shouldPlayNextTrack; }

    // Gets progress into song in seconds
    float PlaybackProgress() const;
    void PlaybackStart();
    void PlaybackPause();
    // Stop playing audio and unload the file
    void PlaybackStop();
    // Seek to a new position in the song
    void PlaybackSeek(float timestamp);

    // Play the track given in info
    // returns 0 on success
    int PlayTrack(std::string file);

    SampleBuffer sampleBuffers[StreamContext_NUM_SAMPLE_BUFFERS];
    ssize_t samplesPerBuffer;
    // Ring buffer of SampleBuffers
    // Index of the last valid sample buffer
    int lastSampleBuffer;
    // The currently processed sample buffer
    int currentSampleBuffer;
    std::mutex sampleBuffersLock;
    // Decoder waits for a buffer to be processed
    std::condition_variable decoderWaitCondition;
    // Decoder thread will block whilst it is not decoding an audio file
    std::condition_variable decoderShouldRunCondition;
    // Player thread will block whilst it is not playing audio samples
    std::condition_variable playerShouldRunCondition;
    int numValidBuffers;

    void(*FlipBuffers)();

private:
    // Decoder Loop
    void Decode();
    void DecodeAudio(struct AVPacket* packet);
    void DecodeVideo(struct AVPacket* packet);
    // Decodes a frame of audio and fills the next available buffer
    void DecoderDecodeFrame(struct AVFrame* frame);
    // Perform the requested seek to m_seekTimestamp
    void DecoderDoSeek();

    // Audio playeer loop.
    // Reads buffers from sampleBuffers and sends them to the audio device
    void PlayAudio();
    
    // Get the next buffer that can hold samplesToWrite number of samples
    // if the buffer after the write pointer is too full,
    // increments the write pointer and returns the buffer after.
    int DecoderGetNextSampleBufferOrWait(int samplesToWrite);

    // A packet is considered invalid if we need to seek
    // or the decoder is not marked as running
    inline bool IsDecoderPacketInvalid() {
        return m_requestSeek || !m_isDecoderRunning;
    }

    inline void FlushSampleBuffers() {
        for (int i = 0; i < StreamContext_NUM_SAMPLE_BUFFERS; i++) {
            sampleBuffers[i].samples = 0;
            sampleBuffers[i].timestamp = 0;
        }
    }

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
    // Lock for m_shouldPlayAudio
    std::mutex m_playerStatusLock;
    // Lock for m_isDecoderRunning
    std::mutex m_decoderStatusLock;

    bool m_shouldThreadsDie = false;
    bool m_isDecoderRunning = false;
    bool m_shouldPlayNextTrack = false;
    // Indicates to the player thread whether to not to play the audio
    bool m_shouldPlayAudio = true;
    
    bool m_requestSeek = false;
    // Timestamp in seconds of where to seek to
    float m_seekTimestamp;
    // Last timestamp played by the playback thread
    float m_lastTimestamp;

    struct AVFormatContext* m_avfmt = nullptr;
    struct AVCodecContext* m_acodec = nullptr;
    struct AVCodecContext* m_vcodec = nullptr;
    struct SwrContext* m_resampler = nullptr;
    struct SwsContext* m_rescaler = nullptr;

    struct AVStream* m_videoStream = nullptr;
    int m_videoStreamIndex = 0;
    struct AVStream* m_audioStream = nullptr;
    int m_audioStreamIndex = 0;

    struct Surface* m_surface;
};
