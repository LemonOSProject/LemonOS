#include "AudioContext.h"

#include <Lemon/Core/Logger.h>
#include <Lemon/System/ABI/Audio.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <time.h>

#include <sys/ioctl.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

// Repsonible for sending samples to the audio driver
void PlayAudio(AudioContext* ctx) {
    // The audio file to be played
    int fd = ctx->m_pcmOut;

    // The amount of channels supported by the audio device
    // Generally will be stereo audio (2 channel)
    int channels = ctx->m_pcmChannels;
    // The size of one audio sample in bytes
    // In this case bit depth / 8
    int sampleSize = ctx->m_pcmBitDepth / 8;

    AudioContext::SampleBuffer* buffers = ctx->sampleBuffers;

    while (!ctx->m_shouldThreadsDie) {
        if(!ctx->m_shouldPlayAudio) {
            std::unique_lock lockStatus{ctx->m_playerStatusLock};
            ctx->playerShouldRunCondition.wait(lockStatus,
                                                [ctx]() -> bool { return ctx->m_shouldPlayAudio; });
        }

        // If the decoder has stopped, exit this loop
        while (ctx->m_shouldPlayAudio && ctx->m_isDecoderRunning) {
            // If there aren't any valid audio buffers,
            // wait for the decoder to catch up
            if (!ctx->numValidBuffers) {
                // Instead of busy waiting just sleep 10ms
                // as not to chew through CPU time
                usleep(100);
                continue;
            };

            // Get the next sample buffer,
            // wrap around at the end (at AUDIOCONTEXT_NUM_SAMPLE_BUFFERS)
            ctx->currentSampleBuffer = (ctx->currentSampleBuffer + 1) % AUDIOCONTEXT_NUM_SAMPLE_BUFFERS;

            auto& buffer = buffers[ctx->currentSampleBuffer];
            ctx->m_lastTimestamp = buffer.timestamp;

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
            std::unique_lock lock{ctx->sampleBuffersLock};
            // If the packet became invalid, numValidBuffers will become 0
            // the packet will become in
            if (ctx->numValidBuffers > 0) {
                ctx->numValidBuffers--;
            }
            lock.unlock();

            // Let the decoder know that we have processed a buffer
            ctx->decoderWaitCondition.notify_all();
        }
    }
}

// Turns encoded audio data (such as MPEG-2 or FLAC) into raw audio samples
void DecodeAudio(AudioContext* ctx) {
    // Start the player thread
    // The player thread sends audio to the driver
    // whilst this thread decodes the audio data
    std::thread playerThread(PlayAudio, ctx);

    while (!ctx->m_shouldThreadsDie) {
        {
            std::unique_lock lockStatus{ctx->m_decoderStatusLock};
            ctx->decoderShouldRunCondition.wait(lockStatus,
                                                [ctx]() -> bool { return ctx->m_isDecoderRunning; });
        }

        ctx->m_decoderLock.lock();
        SwrContext* resampler = swr_alloc();

        // Check how many channels are in the audio file
        if (ctx->m_avcodec->channels == 1) {
            av_opt_set_int(resampler, "in_channel_layout", AV_CH_LAYOUT_MONO, 0);
        } else {
            if (ctx->m_avcodec->channels != 2) {
                Lemon::Logger::Warning("Unsupported number of audio channels {}, taking first 2 and playing as stereo.",
                                       ctx->m_avcodec->channels);
            }

            av_opt_set_int(resampler, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        }
        av_opt_set_int(resampler, "in_sample_rate", ctx->m_avcodec->sample_rate, 0);
        av_opt_set_sample_fmt(resampler, "in_sample_fmt", ctx->m_avcodec->sample_fmt, 0);

        // Check the channel count of the audio device,
        // probably stereo (2 channel)
        if (ctx->m_pcmChannels == 1) {
            av_opt_set_int(resampler, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
        } else {
            av_opt_set_int(resampler, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        }

        // Get the sample rate of the audio device
        // (amount of audio samples processed in one second)
        // For the AC97 audio hardware this will be 48000 Hz
        av_opt_set_int(resampler, "out_sample_rate", ctx->m_pcmSampleRate, 0);

        // Output is signed 16-bit PCM packed
        av_opt_set_sample_fmt(resampler, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
        assert(ctx->m_pcmBitDepth == 16);
        int outputSampleSize = 16 / 8;

        // Reset the sample buffer read and write indexes
        ctx->lastSampleBuffer = 0;
        ctx->currentSampleBuffer = 0;
        ctx->numValidBuffers = 0;

        // Reset all sample buffers,
        // some may still contain old audio data
        ctx->FlushSampleBuffers();

        // Called on every return path
        auto cleanup = [&]() {
            // Set the decoder as not running
            std::unique_lock lockStatus{ctx->m_decoderStatusLock};
            ctx->m_isDecoderRunning = false;
            ctx->numValidBuffers = 0;

            // Free the resampler
            swr_free(&resampler);

            // Free the codec and format contexts
            avcodec_free_context(&ctx->m_avcodec);

            avformat_free_context(ctx->m_avfmt);
            ctx->m_avfmt = nullptr;

            // Unlock the decoder lock letting the other threads
            // know this thread is almost done
            ctx->m_decoderLock.unlock();
        };

        if (swr_init(resampler)) {
            fprintf(stderr, "Could not initialize software resampler\n");

            cleanup();
            continue;
        }

        // Get the player thread to start playing audio
        ctx->PlaybackStart();

        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();

        int frameResult = 0;
        while (ctx->m_isDecoderRunning && (frameResult = av_read_frame(ctx->m_avfmt, packet)) >= 0) {
            if (packet->stream_index != ctx->m_currentStreamIndex) {
                // May be a video stream such as album art, drop it
                av_packet_unref(packet);
                continue;
            }

            // Send the packet to the decoder
            if (int ret = avcodec_send_packet(ctx->m_avcodec, packet); ret) {
                Lemon::Logger::Error("Could not send packet for decoding");
                break;
            }

            // A packet is considered invalid if we need to seek
            // or the decoder is not marked as running
            auto isPacketInvalid = [ctx]() -> bool { return ctx->m_requestSeek || !ctx->m_isDecoderRunning; };

            ssize_t ret = 0;
            while (!isPacketInvalid() && ret >= 0) {
                // Decodes the audio
                ret = avcodec_receive_frame(ctx->m_avcodec, frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    // Get the next packet and retry
                    break;
                } else if (ret) {
                    Lemon::Logger::Error("Could not decode frame", ret);
                    // Stop decoding audio
                    ctx->m_isDecoderRunning = false;
                    break;
                }

                int nextValidBuffer;
                auto hasBuffer = [&]() -> bool {
                    return nextValidBuffer != ctx->currentSampleBuffer || !ctx->m_isDecoderRunning ||
                           ctx->numValidBuffers == 0 || isPacketInvalid();
                };

                nextValidBuffer = ctx->DecoderNextBuffer();

                // Prevent the player thread from taking another buffer until the new audio data
                // is added
                std::unique_lock lock{ctx->sampleBuffersLock};
                ctx->decoderWaitCondition.wait(lock, hasBuffer);

                // If a seek has been requested or m_decoderIsRunning has been set to false
                // the current packet is no longer valid
                if (isPacketInvalid()) {
                    av_frame_unref(frame);
                    break;
                }

                auto* buffer = &ctx->sampleBuffers[nextValidBuffer];
                // Get the amount of audio samples which will be produced by the audio sampler
                // as the sample rate of the output device likely does not match
                // the source audio.
                int samplesToWrite =
                    av_rescale_rnd(swr_get_delay(resampler, ctx->m_avcodec->sample_rate) + frame->nb_samples,
                                   ctx->m_pcmSampleRate, ctx->m_avcodec->sample_rate, AV_ROUND_UP);

                // Check if we can fit our samples in the current sample buffer
                // Increment lastSampleBuffer if the buffer is full
                while (samplesToWrite + buffer->samples > ctx->samplesPerBuffer) {
                    ctx->lastSampleBuffer = nextValidBuffer;
                    ctx->numValidBuffers++;

                    nextValidBuffer = ctx->DecoderNextBuffer();
                    // hasBuffer checks if there is a free sample buffer to take.
                    // Otherwise, release the lock until hasBuffer returns true
                    // so the player thread can process the existing audio in the buffers
                    ctx->decoderWaitCondition.wait(lock, hasBuffer);
                    if (isPacketInvalid()) {
                        av_frame_unref(frame);
                        break;
                    }

                    buffer = &ctx->sampleBuffers[nextValidBuffer];
                }
                // We have found a buffer, let the player thread continue
                lock.unlock();

                // Get the position in the buffer
                uint8_t* outputData = buffer->data + (buffer->samples * outputSampleSize) * 2;
                // Resample the audio to match the output device
                int samplesWritten = swr_convert(resampler, &outputData, (int)(ctx->samplesPerBuffer - buffer->samples),
                                                 (const uint8_t**)frame->extended_data, frame->nb_samples);
                buffer->samples += samplesWritten;

                // Set the timestamp for the buffer
                buffer->timestamp = frame->best_effort_timestamp / ctx->m_currentStream->time_base.den;
                av_frame_unref(frame);
            }

            av_packet_unref(packet);

            // If the decoder should still run
            // and m_requestSeek is true, seek to a new point in the audio
            if (ctx->m_isDecoderRunning && ctx->m_requestSeek) {
                // Since all buffers need to be reset to prevent playing old audio,
                // lock the sample buffers so the player thread does not interfere
                std::scoped_lock lock{ctx->sampleBuffersLock};
                ctx->numValidBuffers = 0;
                ctx->lastSampleBuffer = ctx->currentSampleBuffer;
                // Set the amount of samples in each buffer to zero
                ctx->FlushSampleBuffers();

                // Flush the decoder and resampler buffers
                avcodec_flush_buffers(ctx->m_avcodec);
                swr_convert(resampler, NULL, 0, NULL, 0);

                // Seek to the new timestamp
                float timestamp = ctx->m_seekTimestamp;
                av_seek_frame(ctx->m_avfmt, ctx->m_currentStreamIndex,
                              (long)(timestamp / av_q2d(ctx->m_currentStream->time_base)), 0);

                ctx->m_lastTimestamp = ctx->m_seekTimestamp;
                // Set m_requestSeek to false indicating that seeking has finished
                ctx->m_requestSeek = false;
            }
        }

        // If we got to the end of file (did not encounter errors)
        // let the main thread know to play the next track
        if (frameResult == AVERROR_EOF) {
            // We finished playing
            ctx->m_shouldPlayNextTrack = true;
        }
        cleanup();
    }

    // Wait for playerThread to finish
    playerThread.join();
}

AudioContext::AudioContext() {
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
    for (int i = 0; i < AUDIOCONTEXT_NUM_SAMPLE_BUFFERS; i++) {
        sampleBuffers[i].data = new uint8_t[samplesPerBuffer * m_pcmChannels * (m_pcmBitDepth / 8)];
        sampleBuffers[i].samples = 0;
    }

    m_decoderThread = std::thread(DecodeAudio, this);
}

AudioContext::~AudioContext() {
    m_shouldThreadsDie = true;

    // Wait for playback to stop before exiting
    PlaybackStop();
}

float AudioContext::PlaybackProgress() const {
    if (!m_isDecoderRunning) {
        return 0;
    }

    // Return timestamp of last played buffer
    return m_lastTimestamp;
}

void AudioContext::PlaybackStart() {
    std::unique_lock lockStatus{m_playerStatusLock};
    m_shouldPlayAudio = true;
    playerShouldRunCondition.notify_all();
}

void AudioContext::PlaybackPause() {
    std::unique_lock lockStatus{m_playerStatusLock};
    m_shouldPlayAudio = false;
}

void AudioContext::PlaybackStop() {
    if (m_isDecoderRunning) {
        // Mark the decoder as not running
        std::unique_lock lockDecoderStatus{m_decoderStatusLock};
        m_isDecoderRunning = false;
        m_shouldPlayAudio = false;
        // Make the decoder stop waiting for a free sample buffer
        decoderWaitCondition.notify_all();
    }

    // Only set m_currentTrack in the main thread
    // to avoid race conditions and worrying about synchronization
    m_currentTrack = nullptr;
}

void AudioContext::PlaybackSeek(float timestamp) {
    if (m_isDecoderRunning) {
        m_seekTimestamp = timestamp;
        m_requestSeek = true;

        // Let the decoder thread know that we want to seek
        decoderWaitCondition.notify_all();
        while (m_requestSeek)
            usleep(1000); // Wait for seek to finish, just wait 1ms as not to hog the CPU
    }
}

int AudioContext::PlayTrack(TrackInfo* info) {
    // Stop any audio currently playing
    if (m_isDecoderRunning) {
        PlaybackStop();
    }

    // Block the decoder from running until we are done here
    std::lock_guard lockDecoder(m_decoderLock);

    assert(!m_avfmt);
    m_avfmt = avformat_alloc_context();

    // Opens the audio file
    if (int err = avformat_open_input(&m_avfmt, info->filepath.c_str(), NULL, NULL); err) {
        Lemon::Logger::Error("Failed to open {}", info->filepath);
        return err;
    }

    // Gets metadata and information about the audio contained in the file
    if (int err = avformat_find_stream_info(m_avfmt, NULL); err) {
        Lemon::Logger::Error("Failed to get stream info for {}", info->filepath);
        return err;
    }

    // Update track metadata in case the file has changed
    GetTrackInfo(m_avfmt, info);

    // Data is organised into 'streams'
    // Search for the 'best' audio stream
    // Any album art may be placed in a video stream
    int streamIndex = av_find_best_stream(m_avfmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (streamIndex < 0) {
        Lemon::Logger::Error("Failed to get audio stream for {}", info->filepath);
        return streamIndex;
    }

    m_currentStream = m_avfmt->streams[streamIndex];
    m_currentStreamIndex = streamIndex;

    // Find a decoder for the audio data we have been given
    const AVCodec* decoder = avcodec_find_decoder(m_currentStream->codecpar->codec_id);
    if (!decoder) {
        Lemon::Logger::Error("Failed to find codec for '{}'", info->filepath);
        return 1;
    }

    m_avcodec = avcodec_alloc_context3(decoder);
    assert(decoder);

    if (avcodec_parameters_to_context(m_avcodec, m_currentStream->codecpar)) {
        Lemon::Logger::Error("Failed to initialie codec context.");
        return 1;
    }

    if (avcodec_open2(m_avcodec, decoder, NULL) < 0) {
        Lemon::Logger::Error("Failed to open codec!");
        return 1;
    }

    // Set the current track so it can be accessed by PlayerWidget
    m_currentTrack = info;

    m_requestSeek = false;
    m_shouldPlayNextTrack = false;

    // Notify the decoder thread and mark the decoder as running
    std::scoped_lock lockDecoderStatus{m_decoderStatusLock};
    m_isDecoderRunning = true;
    decoderShouldRunCondition.notify_all();
    return 0;
}

int AudioContext::LoadTrack(std::string filepath, TrackInfo* info) {
    AVFormatContext* fmt = avformat_alloc_context();
    if (int r = avformat_open_input(&fmt, filepath.c_str(), NULL, NULL); r) {
        Lemon::Logger::Error("Failed to open {}", filepath);
        return r;
    }

    // Read the metadata, etc. of the file
    if (int r = avformat_find_stream_info(fmt, NULL); r) {
        avformat_free_context(fmt);
        return r;
    }

    // Fill the TrackInfo
    info->filepath = filepath;

    // Get the filename by searching for the last slash in the filepath
    // and taking everything after that
    // e.g. /system/audio.wav becomes audio.wav
    if (size_t lastSeparator = info->filepath.find_last_of('/'); lastSeparator != std::string::npos) {
        info->filename = filepath.substr(lastSeparator + 1);
    } else {
        info->filename = filepath;
    }

    GetTrackInfo(fmt, info);

    avformat_free_context(fmt);
    return 0;
}

void AudioContext::GetTrackInfo(struct AVFormatContext* fmt, TrackInfo* info) {
    // Get the duration (given in microseconds) in seconds
    float durationSeconds = fmt->duration / 1000000.f;
    info->duration = durationSeconds;
    info->durationString = fmt::format("{:02}:{:02}", (int)durationSeconds / 60, (int)durationSeconds % 60);

    // Get file metadata using libavformat
    AVDictionaryEntry* tag = nullptr;
    tag = av_dict_get(fmt->metadata, "title", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        info->metadata.title = tag->value;
    } else {
        info->metadata.title = "Unknown";
    }

    tag = av_dict_get(fmt->metadata, "artist", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        info->metadata.artist = tag->value;
    } else {
        info->metadata.artist = "";
    }

    tag = av_dict_get(fmt->metadata, "album", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        info->metadata.album = tag->value;
    } else {
        info->metadata.album = "";
    }

    tag = av_dict_get(fmt->metadata, "date", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        // First 4 digits are year
        info->metadata.year = std::string(tag->value).substr(0, 4);
    }
}
