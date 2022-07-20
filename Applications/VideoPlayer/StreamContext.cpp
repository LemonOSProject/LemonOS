#include "StreamContext.h"

#include <Lemon/Core/Logger.h>
#include <Lemon/Graphics/Surface.h>
#include <Lemon/System/ABI/Audio.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <time.h>

#include <sys/ioctl.h>

// The libav* libraries do not add extern "C" when using C++,
// so specify here that all functions are C functions and do not have mangled names
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

// The default audio output device is a file at '/dev/snd/pcm'
//
// open() is called to get a handle to the device
//
// ioctl() is used to get the device configuration including:
//  - Number of channels
//  - Audio sample format
//  - Audio sample rate
//
// For example, to get the sample rate:
// int pcmSampleRate = ioctl(pcmOut, IoCtlOutputGetSampleRate);
//
// Where pcmOut is the file descriptor/handle pointing to the device.
//
// To actually play the audio, a call to write() is used.
// Samples are written using write() to the device file
// which passes sample data to the audio driver.
//
// Audio samples are read sequentially, in a FIFO (first-in first-out) manner

// Repsonible for sending samples to the audio driver
void StreamContext::PlayAudio() {
    // The audio file to be played
    int fd = m_pcmOut;

    // The amount of channels supported by the audio device
    // Generally will be stereo audio (2 channel)
    int channels = m_pcmChannels;
    // The size of one audio sample in bytes
    // In this case bit depth / 8
    int sampleSize = m_pcmBitDepth / 8;

    StreamContext::SampleBuffer* buffers = sampleBuffers;

    while (!m_shouldThreadsDie) {
        if (!m_shouldPlayAudio) {
            std::unique_lock lockStatus{m_playerStatusLock};
            playerShouldRunCondition.wait(lockStatus, [this]() -> bool { return m_shouldPlayAudio; });
        }

        // If the decoder has stopped, exit this loop
        while (m_shouldPlayAudio && m_isDecoderRunning) {
            // If there aren't any valid audio buffers,
            // wait for the decoder to catch up
            if (!numValidBuffers) {
                // Instead of busy waiting just sleep 5ms
                // as not to chew through CPU time
                usleep(5000);
                continue;
            };

            // Get the next sample buffer,
            // wrap around at the end (at StreamContext_NUM_SAMPLE_BUFFERS)
            currentSampleBuffer = (currentSampleBuffer + 1) % StreamContext_NUM_SAMPLE_BUFFERS;

            auto& buffer = buffers[currentSampleBuffer];
            m_lastTimestamp = buffer.timestamp;

            // Write the buffer to the device file
            // If the buffer queue is full,
            // waiting for the audio hardware to process the audio,
            // this call to write will block program execution until
            // it has written all the data
            int ret = write(fd, buffer.data, buffer.samples * channels * sampleSize);
            if (ret < 0) {
                Lemon::Logger::Warning("/snd/dev/pcm: Error writing samples: {}", strerror(errno));
            }

            // Now that the buffer has been processed by the audio buffer,
            // set the number of samples in the buffer
            buffer.samples = 0;

            // Make sure the decoder thread does not interfere
            std::unique_lock lock{sampleBuffersLock};
            // If the packet became invalid, numValidBuffers will become 0
            // the packet will become in
            if (numValidBuffers > 0) {
                numValidBuffers--;
            }
            lock.unlock();

            // Let the decoder know that we have processed a buffer
            decoderWaitCondition.notify_all();
        }
    }
}

StreamContext::StreamContext() {
    // Open the PCM output device file
    // When audio samples are written to this file
    // they get sent to the audio driver
    m_pcmOut = open("/dev/snd/pcm", O_WRONLY);
    if (m_pcmOut < 0) {
        Lemon::Logger::Error("Failed to open PCM output '/dev/snd/pcm': {}", strerror(errno));
        exit(1);
    }

    // Get output information

    // - Find the amount of audio channels (e.g. mono audio, stereo audio)
    m_pcmChannels = ioctl(m_pcmOut, IoCtlOutputGetNumberOfChannels);
    if (m_pcmChannels <= 0) {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputGetNumberOfChannels: {}", strerror(errno));
        exit(1);
    }

    // AudioPlayer does not support more than two channel (stereo) audio
    assert(m_pcmChannels == 1 || m_pcmChannels == 2);

    // - Get the data encoding that the audio driver wants
    int outputEncoding = ioctl(m_pcmOut, IoCtlOutputGetEncoding);
    if (outputEncoding < 0) {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputGetEncoding: {}", strerror(errno));
        exit(1);
    }

    // Check its a supported encoding
    if (outputEncoding == PCMS16LE) {
        m_pcmBitDepth = 16;
    } else if (outputEncoding == PCMS20LE) {
        m_pcmBitDepth = 20;
    } else {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputGetEncoding: Encoding not supported");
    }

    // Whilst we are aware of 20-bit audio output,
    // it is not well supported by hardware and software including
    // the resampler that is used (libswresample library)
    if (m_pcmBitDepth != 16) {
        Lemon::Logger::Error("/dev/snd/pcm Unsupported PCM sample depth of {}", m_pcmBitDepth);
        exit(1);
    }

    // Get the sample rate (e.g. 48000Hz)
    // indicating the amount of audio samples
    // that get played in one second
    m_pcmSampleRate = ioctl(m_pcmOut, IoCtlOutputGetSampleRate);
    if (m_pcmSampleRate < 0) {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputGetSampleRate: {}", strerror(errno));
        exit(1);
    }

    // ~100ms of audio per buffer (1 / 10 seconds)
    // samples per buffer = sample rate / 10
    samplesPerBuffer = m_pcmSampleRate / 10;

    // Initialize the sample buffers
    // allocating the array and setting the amount of samples to 0
    for (int i = 0; i < StreamContext_NUM_SAMPLE_BUFFERS; i++) {
        sampleBuffers[i].data = new uint8_t[samplesPerBuffer * m_pcmChannels * (m_pcmBitDepth / 8)];
        sampleBuffers[i].samples = 0;
    }

    m_decoderThread = std::thread(&StreamContext::Decode, this);
}

StreamContext::~StreamContext() {
    m_shouldThreadsDie = true;

    // Wait for playback to stop before exiting
    PlaybackStop();
}

void StreamContext::SetDisplaySurface(Surface* s, Rect blitRect) {
    std::unique_lock lockStatus{m_decoderStatusLock};

    m_surface = s;
    m_surfaceBlitRegion = blitRect;

    if (m_isDecoderRunning) {
        InitializeRescaler();
    }
}

void StreamContext::DecodeVideo(AVPacket* packet) {
    AVFrame* frame = av_frame_alloc();

    // Send the packet to the decoder
    if (int ret = avcodec_send_packet(m_vcodec, packet); ret) {
        Lemon::Logger::Error("Could not send packet for decoding");
        return;
    }

    ssize_t ret = 0;
    while (!IsDecoderPacketInvalid() && ret >= 0) {
        // Decodes the audio
        ret = avcodec_receive_frame(m_vcodec, frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            // Get the next packet and retry
            break;
        } else if (ret) {
            Lemon::Logger::Error("Could not decode frame: {}", ret);
            // Stop decoding audio
            m_isDecoderRunning = false;
            break;
        }

        std::unique_lock lockSurface{surfaceLock};

        // Decode video frame
        int stride = 4 * m_surface->width;
        uint8_t* buffer = m_surface->buffer + m_surfaceBlitRegion.y * stride + m_surfaceBlitRegion.x * 4;

        sws_scale(m_rescaler, frame->data, frame->linesize, 0, m_vcodec->height, &buffer, &stride);
        FlipBuffers();

        av_frame_unref(frame);
    }

    av_packet_unref(packet);
}

// Turns encoded audio data (such as MPEG-2 or FLAC) into raw audio samples
void StreamContext::DecodeAudio(AVPacket* packet) {
    AVFrame* frame = av_frame_alloc();

    // Send the packet to the decoder
    if (int ret = avcodec_send_packet(m_acodec, packet); ret) {
        Lemon::Logger::Error("Could not send packet for decoding");
        return;
    }

    ssize_t ret = 0;
    while (!IsDecoderPacketInvalid() && ret >= 0) {
        // Decodes the audio
        ret = avcodec_receive_frame(m_acodec, frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            // Get the next packet and retry
            break;
        } else if (ret) {
            Lemon::Logger::Error("Could not decode frame: {}", ret);
            // Stop decoding audio
            m_isDecoderRunning = false;
            break;
        }

        DecoderDecodeFrame(frame);

        av_frame_unref(frame);
    }

    av_packet_unref(packet);

    // If the decoder should still run
    // and m_requestSeek is true, seek to a new point in the audio
    if (m_isDecoderRunning && m_requestSeek) {
        DecoderDoSeek();
    }
}

void StreamContext::Decode() {
    // Start the player thread
    // The player thread sends audio to the driver
    // whilst this thread decodes the audio data
    std::thread playerThread(&StreamContext::PlayAudio, this);

    while (!m_shouldThreadsDie) {
        {
            std::unique_lock lockStatus{m_decoderStatusLock};
            decoderShouldRunCondition.wait(lockStatus, [this]() -> bool { return m_isDecoderRunning; });
        }

        m_decoderLock.lock();
        m_resampler = swr_alloc();

        surfaceLock.lock();
        InitializeRescaler();
        surfaceLock.unlock();

        // Check how many channels are in the audio file
        if (m_acodec->channels == 1) {
            av_opt_set_int(m_resampler, "in_channel_layout", AV_CH_LAYOUT_MONO, 0);
        } else {
            if (m_acodec->channels != 2) {
                Lemon::Logger::Warning("Unsupported number of audio channels {}, taking first 2 and playing as stereo.",
                                       m_acodec->channels);
            }

            av_opt_set_int(m_resampler, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        }
        av_opt_set_int(m_resampler, "in_sample_rate", m_acodec->sample_rate, 0);
        av_opt_set_sample_fmt(m_resampler, "in_sample_fmt", m_acodec->sample_fmt, 0);

        // Check the channel count of the audio device,
        // probably stereo (2 channel)
        if (m_pcmChannels == 1) {
            av_opt_set_int(m_resampler, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
        } else {
            av_opt_set_int(m_resampler, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        }

        // Get the sample rate of the audio device
        // (amount of audio samples processed in one second)
        // For the AC97 audio hardware this will be 48000 Hz
        av_opt_set_int(m_resampler, "out_sample_rate", m_pcmSampleRate, 0);

        // Output is signed 16-bit PCM packed
        av_opt_set_sample_fmt(m_resampler, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
        assert(m_pcmBitDepth == 16);

        // Reset the sample buffer read and write indexes
        lastSampleBuffer = 0;
        currentSampleBuffer = 0;
        numValidBuffers = 0;

        // Reset all sample buffers,
        // some may still contain old audio data
        FlushSampleBuffers();

        if (swr_init(m_resampler)) {
            // If we failed to initialize the resampler,
            // set decoder as not running, preventing any audio from being decoded
            Lemon::Logger::Error("Could not initialize software resampler");

            std::unique_lock lockStatus{m_decoderStatusLock};
            m_isDecoderRunning = false;
        } else {
            // Get the player thread to start playing audio
            PlaybackStart();
        }

        AVPacket* packet = av_packet_alloc();

        int frameResult = 0;
        while (m_isDecoderRunning && (frameResult = av_read_frame(m_avfmt, packet)) >= 0) {
            if (packet->stream_index == m_videoStreamIndex) {
                DecodeVideo(packet);
            } else if (packet->stream_index == m_audioStreamIndex) {
                DecodeAudio(packet);
            } else {
                av_packet_unref(packet);
            }
        }

        // If we got to the end of file (did not encounter errors)
        // let the main thread know to play the next track
        if (frameResult == AVERROR_EOF) {
            // We finished playing
            m_shouldPlayNextTrack = true;
        }

        // Clean up after ourselves
        // Set the decoder as not running
        std::unique_lock lockStatus{m_decoderStatusLock};
        m_isDecoderRunning = false;
        numValidBuffers = 0;

        // Free the resampler
        swr_free(&m_resampler);
        m_resampler = nullptr;

        // Free the codec and format contexts
        avcodec_free_context(&m_acodec);
        avcodec_free_context(&m_vcodec);

        avformat_free_context(m_avfmt);
        m_avfmt = nullptr;

        // Unlock the decoder lock letting the other threads
        // know this thread is almost done
        m_decoderLock.unlock();
    }

    // Wait for playerThread to finish
    playerThread.join();
}

void StreamContext::DecoderDecodeFrame(AVFrame* frame) {
    // Bytes per audio sample
    int outputSampleSize = (m_pcmBitDepth / 8);

    // If a seek has been requested or m_decoderIsRunning has been set to false
    // the current packet is no longer valid
    if (IsDecoderPacketInvalid()) {
        return;
    }

    // Get the amount of audio samples which will be produced by the audio sampler
    // as the sample rate of the output device likely does not match
    // the source audio.
    int samplesToWrite = av_rescale_rnd(swr_get_delay(m_resampler, m_acodec->sample_rate) + frame->nb_samples,
                                        m_pcmSampleRate, m_acodec->sample_rate, AV_ROUND_UP);

    int bufferIndex = DecoderGetNextSampleBufferOrWait(samplesToWrite);
    if (bufferIndex < 0) {
        // DecoderGetNextSampleBufferOrWait will return a value < 0
        // if the packet is invalid, so just return
        return;
    }

    auto* buffer = &sampleBuffers[bufferIndex];

    // Get the position in the buffer:
    // samples * outputSampleSize * pcmChannels
    // Where samples is the number of samples per channel,
    // outputSampleSize is the size of each sample in bytes
    // and pcmChannels is the number of channels for the audio output
    uint8_t* outputData = buffer->data + (buffer->samples * outputSampleSize) * m_pcmChannels;

    // Resample the audio to match the output device
    int samplesWritten = swr_convert(m_resampler, &outputData, (int)(samplesPerBuffer - buffer->samples),
                                     (const uint8_t**)frame->extended_data, frame->nb_samples);
    buffer->samples += samplesWritten;

    // Set the timestamp for the buffer
    buffer->timestamp = frame->best_effort_timestamp / m_audioStream->time_base.den;
}

int StreamContext::DecoderGetNextSampleBufferOrWait(int samplesToWrite) {
    // Sample buffers are in a ring buffer, which is circular queue of buffers.
    // This means that when the end of the queue is reached,
    // we wrap around to the beginning.
    int nextValidBuffer = (lastSampleBuffer + 1) % StreamContext_NUM_SAMPLE_BUFFERS;

    // Checks whether or not to keep waiting for a buffer.
    // Checks for three cases.
    //
    // 1. Checks if the next valid buffer (buffer after lastSampleBuffer) is
    // not ahead of PlayAudio, meaning the buffers are not full.
    // 2. numValidBuffers is 0 meaning that there is no audio data at all in the buffers
    // 3. The current packet is invalid
    auto hasBuffer = [&]() -> bool {
        return nextValidBuffer != currentSampleBuffer || numValidBuffers == 0 || IsDecoderPacketInvalid();
    };

    // Prevent the player thread from taking another buffer until the new audio data
    // is added
    std::unique_lock lock{sampleBuffersLock};
    // Lock the sample buffers, check if hasBuffer() is true, if it is false release the lock
    // and wait.
    decoderWaitCondition.wait(lock, hasBuffer);

    // If a seek has been requested or m_decoderIsRunning has been set to false
    // the current packet is no longer valid
    if (IsDecoderPacketInvalid()) {
        return -1;
    }

    auto* buffer = &sampleBuffers[nextValidBuffer];

    // Check if we can fit our samples in the current sample buffer
    // Increment lastSampleBuffer if the buffer is full
    if (samplesToWrite + buffer->samples > samplesPerBuffer) {
        lastSampleBuffer = nextValidBuffer;
        numValidBuffers++;

        nextValidBuffer = (lastSampleBuffer + 1) % StreamContext_NUM_SAMPLE_BUFFERS;
        // hasBuffer checks if nextValidBuffer is a free sample buffer to take.
        // Otherwise, release the lock until hasBuffer returns true
        // so the player thread can process the existing audio in the buffers
        decoderWaitCondition.wait(lock, hasBuffer);
        if (IsDecoderPacketInvalid()) {
            return -1;
        }
    }

    // We have found a buffer, let the player thread continue
    return nextValidBuffer;
}

void StreamContext::DecoderDoSeek() {
    assert(m_requestSeek);

    // Since all buffers need to be reset to prevent playing old audio,
    // lock the sample buffers so the player thread does not interfere
    std::scoped_lock lock{sampleBuffersLock};
    numValidBuffers = 0;
    lastSampleBuffer = currentSampleBuffer;
    // Set the amount of samples in each buffer to zero
    FlushSampleBuffers();

    // Flush the decoder and resampler buffers
    avcodec_flush_buffers(m_acodec);
    swr_convert(m_resampler, NULL, 0, NULL, 0);

    // Seek to the new timestamp
    float timestamp = m_seekTimestamp;
    av_seek_frame(m_avfmt, m_audioStreamIndex, (long)(timestamp / av_q2d(m_audioStream->time_base)), 0);

    m_lastTimestamp = m_seekTimestamp;
    // Set m_requestSeek to false indicating that seeking has finished
    m_requestSeek = false;
}

void StreamContext::InitializeRescaler() {
    assert(m_isDecoderRunning && m_vcodec);

    if (m_rescaler) {
        sws_freeContext(m_rescaler);
    }

    m_rescaler = sws_getContext(m_vcodec->width, m_vcodec->height, m_vcodec->pix_fmt, m_surface->width,
                                m_surface->height, AV_PIX_FMT_RGB32, SWS_BILINEAR, NULL, NULL, NULL);
}

float StreamContext::PlaybackProgress() const {
    if (!m_isDecoderRunning) {
        return 0;
    }

    // Return timestamp of last played buffer
    return m_lastTimestamp;
}

void StreamContext::PlaybackStart() {
    std::unique_lock lockStatus{m_playerStatusLock};
    m_shouldPlayAudio = true;
    playerShouldRunCondition.notify_all();
}

void StreamContext::PlaybackPause() {
    std::unique_lock lockStatus{m_playerStatusLock};
    m_shouldPlayAudio = false;
}

void StreamContext::PlaybackStop() {
    if (m_isDecoderRunning) {
        // Mark the decoder as not running
        std::unique_lock lockDecoderStatus{m_decoderStatusLock};
        m_isDecoderRunning = false;
        m_shouldPlayAudio = false;
        // Make the decoder stop waiting for a free sample buffer
        decoderWaitCondition.notify_all();
    }
}

void StreamContext::PlaybackSeek(float timestamp) {
    if (m_isDecoderRunning) {
        m_seekTimestamp = timestamp;
        m_requestSeek = true;

        // Let the decoder thread know that we want to seek
        decoderWaitCondition.notify_all();
        while (m_requestSeek)
            usleep(1000); // Wait for seek to finish, just wait 1ms as not to hog the CPU
    }
}

int StreamContext::PlayTrack(std::string file) {
    // Stop any audio currently playing
    if (m_isDecoderRunning) {
        PlaybackStop();
    }

    // Block the decoder from running until we are done here
    std::lock_guard lockDecoder(m_decoderLock);

    assert(!m_avfmt);
    m_avfmt = avformat_alloc_context();

    // Opens the audio file
    if (int err = avformat_open_input(&m_avfmt, file.c_str(), NULL, NULL); err) {
        Lemon::Logger::Error("Failed to open {}", file);
        return err;
    }

    // Gets metadata and information about the audio contained in the file
    if (int err = avformat_find_stream_info(m_avfmt, NULL); err) {
        Lemon::Logger::Error("Failed to get stream info for {}", file);
        return err;
    }

    // Update track metadata in case the file has changed
    // GetTrackInfo(m_avfmt, info);

    // Data is organised into 'streams'
    // Search for the 'best' audio stream
    // Any album art may be placed in a video stream
    int streamIndex = av_find_best_stream(m_avfmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (streamIndex < 0) {
        Lemon::Logger::Error("Failed to get audio stream for {}", file);
        return streamIndex;
    }

    m_audioStream = m_avfmt->streams[streamIndex];
    m_audioStreamIndex = streamIndex;

    streamIndex = av_find_best_stream(m_avfmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamIndex < 0) {
        Lemon::Logger::Error("Failed to get video stream for {}", file);
        return streamIndex;
    }
    m_videoStream = m_avfmt->streams[streamIndex];
    m_videoStreamIndex = streamIndex;

    // Find a decoder for the audio data we have been given
    const AVCodec* decoder = avcodec_find_decoder(m_audioStream->codecpar->codec_id);
    if (!decoder) {
        Lemon::Logger::Error("Failed to find codec for '{}'", file);
        return 1;
    }

    m_acodec = avcodec_alloc_context3(decoder);
    assert(m_acodec);

    if (avcodec_parameters_to_context(m_acodec, m_audioStream->codecpar)) {
        Lemon::Logger::Error("Failed to initialie codec context.");
        return 1;
    }

    if (avcodec_open2(m_acodec, decoder, NULL) < 0) {
        Lemon::Logger::Error("Failed to open codec!");
        return 1;
    }

    decoder = avcodec_find_decoder(m_videoStream->codecpar->codec_id);
    if (!decoder) {
        Lemon::Logger::Error("Failed to find codec for '{}'", file);
        return 1;
    }

    m_vcodec = avcodec_alloc_context3(decoder);
    assert(m_vcodec);

    if (avcodec_parameters_to_context(m_vcodec, m_videoStream->codecpar)) {
        Lemon::Logger::Error("Failed to initialie codec context.");
        return 1;
    }

    if (avcodec_open2(m_vcodec, decoder, NULL) < 0) {
        Lemon::Logger::Error("Failed to open codec!");
        return 1;
    }

    m_requestSeek = false;
    m_shouldPlayNextTrack = false;

    // Notify the decoder thread and mark the decoder as running
    std::scoped_lock lockDecoderStatus{m_decoderStatusLock};
    m_isDecoderRunning = true;
    decoderShouldRunCondition.notify_all();
    return 0;
}
