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

//#define AUDIOCONTEXT_TIME_PLAYBACK

void PlayAudio(AudioContext* ctx) {
    int fd = ctx->m_pcmOut;

    int channels = ctx->m_pcmChannels;
    int sampleSize = ctx->m_pcmBitDepth / 8;

    AudioContext::SampleBuffer* buffers = ctx->sampleBuffers;
    while (ctx->m_isDecoderRunning) {
        if (!ctx->numValidBuffers) {
            Lemon::Logger::Warning("Decoder running behind!");
            continue;
        };
        ctx->currentSampleBuffer = (ctx->currentSampleBuffer + 1) % AUDIOCONTEXT_NUM_SAMPLE_BUFFERS;

        auto& buffer = buffers[ctx->currentSampleBuffer];
        ctx->m_lastTimestamp = buffer.timestamp;

        int ret = write(fd, buffer.data, buffer.samples * channels * sampleSize);
        if (ret < 0) {
            Lemon::Logger::Warning("/snd/dev/pcm: Error writing samples: {}", strerror(errno));
        }

        buffer.samples = 0;

        std::unique_lock lock{ctx->sampleBuffersLock};
        // If the packet became invalid, numValidBuffers will become 0
        if (ctx->numValidBuffers > 0) {
            ctx->numValidBuffers--;
        }
        lock.unlock();

        // Let the decoder know that we have processed a buffer
        ctx->decoderWaitCondition.notify_all();
    }
}

void DecodeAudio(AudioContext* ctx) {
    ctx->m_decoderLock.lock();
    SwrContext* resampler = swr_alloc();

    av_opt_set_int(resampler, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(resampler, "in_sample_rate", ctx->m_avcodec->sample_rate, 0);
    av_opt_set_sample_fmt(resampler, "in_sample_fmt", ctx->m_avcodec->sample_fmt, 0);

    // Dual channel
    if (ctx->m_pcmChannels == 1) {
        av_opt_set_int(resampler, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
    } else {
        av_opt_set_int(resampler, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    }

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

    std::thread playerThread(PlayAudio, ctx);

    auto cleanup = [&]() {
        ctx->numValidBuffers = 0;

        swr_free(&resampler);

        avcodec_free_context(&ctx->m_avcodec);

        avformat_free_context(ctx->m_avfmt);
        ctx->m_avfmt = nullptr;
        ctx->m_isDecoderRunning = false;

        // Wait for playerThread to finish
        playerThread.join();

        ctx->m_decoderLock.unlock();
    };

    if (swr_init(resampler)) {
        fprintf(stderr, "Could not initialize software resampler\n");

        cleanup();
        return;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    while (ctx->m_isDecoderRunning && av_read_frame(ctx->m_avfmt, packet) >= 0) {
        if (avcodec_send_packet(ctx->m_avcodec, packet)) {
            Lemon::Logger::Error("Could not send packet for decoding");
            cleanup();
            return;
        }

        auto isPacketInvalid = [ctx]() -> bool {
            return ctx->m_requestSeek || !ctx->m_isDecoderRunning;
        };

        ssize_t ret = 0;
        while (!isPacketInvalid() && ret >= 0) {
            ret = avcodec_receive_frame(ctx->m_avcodec, frame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                break;
            } else if (ret) {
                Lemon::Logger::Error("Could not decode frame", ret);
                cleanup();
                return;
            }

            int nextValidBuffer;
            auto hasBuffer = [&]() -> bool {
                return nextValidBuffer != ctx->currentSampleBuffer || !ctx->m_isDecoderRunning ||
                       ctx->numValidBuffers == 0 || isPacketInvalid();
            };

            nextValidBuffer = ctx->DecoderNextBuffer();
            std::unique_lock lock{ctx->sampleBuffersLock};
            ctx->decoderWaitCondition.wait(lock, hasBuffer);

            if (isPacketInvalid()) {
                av_frame_unref(frame);
                break;
            }

            auto* buffer = &ctx->sampleBuffers[nextValidBuffer];
            int samplesToWrite =
                av_rescale_rnd(swr_get_delay(resampler, ctx->m_avcodec->sample_rate) + frame->nb_samples,
                               ctx->m_pcmSampleRate, ctx->m_avcodec->sample_rate, AV_ROUND_UP);

            // Increment lastSampleBuffer if the buffer after is full
            while (samplesToWrite + buffer->samples > ctx->samplesPerBuffer) {
                ctx->lastSampleBuffer = nextValidBuffer;
                ctx->numValidBuffers++;

                nextValidBuffer = ctx->DecoderNextBuffer();
                ctx->decoderWaitCondition.wait(lock, hasBuffer);
                if (isPacketInvalid()) {
                    av_frame_unref(frame);
                    break;
                }

                buffer = &ctx->sampleBuffers[nextValidBuffer];
            }
            lock.unlock();

            uint8_t* outputData = buffer->data + (buffer->samples * outputSampleSize) * 2;
            int samplesWritten = swr_convert(resampler, &outputData, (int)(ctx->samplesPerBuffer - buffer->samples),
                                             (const uint8_t**)frame->extended_data, frame->nb_samples);
            buffer->samples += samplesWritten;

            buffer->timestamp = frame->best_effort_timestamp / ctx->m_currentStream->time_base.den;

            av_frame_unref(frame);
        }

        av_packet_unref(packet);

        if (ctx->m_requestSeek) {
            std::scoped_lock lock{ctx->sampleBuffersLock};
            ctx->numValidBuffers = 0;
            ctx->lastSampleBuffer = ctx->currentSampleBuffer;
            ctx->FlushSampleBuffers();

            float timestamp = ctx->m_seekTimestamp;
            av_seek_frame(ctx->m_avfmt, 0, (long)(timestamp / av_q2d(ctx->m_currentStream->time_base)), 0);

            avcodec_flush_buffers(ctx->m_avcodec);
            swr_convert(resampler, NULL, 0, NULL, 0);

            ctx->m_lastTimestamp = ctx->m_seekTimestamp;
            ctx->m_requestSeek = false;
        }
    }

    cleanup();
    return;
}

AudioContext::AudioContext() {
    // Open the PCM output device
    m_pcmOut = open("/dev/snd/pcm", O_WRONLY);
    if (m_pcmOut < 0) {
        Lemon::Logger::Error("Failed to open PCM output '/dev/snd/pcm': {}", strerror(errno));
        exit(1);
    }

    if (ioctl(m_pcmOut, IoCtlOutputSetAsync, 1)) {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputSetAsync: {}", strerror(errno));
        exit(1);
    }

    // Get output information
    m_pcmChannels = ioctl(m_pcmOut, IoCtlOutputGetNumberOfChannels);
    if (m_pcmChannels <= 0) {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputGetNumberOfChannels: {}", strerror(errno));
        exit(1);
    }
    assert(m_pcmChannels == 1 || m_pcmChannels == 2);

    int outputEncoding = ioctl(m_pcmOut, IoCtlOutputGetEncoding);
    if (outputEncoding < 0) {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputGetEncoding: {}", strerror(errno));
        exit(1);
    }

    if (outputEncoding == PCMS16LE) {
        m_pcmBitDepth = 16;
    } else if (outputEncoding == PCMS20LE) {
        m_pcmBitDepth = 20;
    } else {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputGetEncoding: Encoding not supported");
    }

    m_pcmSampleRate = ioctl(m_pcmOut, IoCtlOutputGetSampleRate);
    if (m_pcmSampleRate < 0) {
        Lemon::Logger::Error("/dev/snd/pcm IoCtlOutputGetSampleRate: {}", strerror(errno));
        exit(1);
    }

    if (m_pcmBitDepth != 16) {
        Lemon::Logger::Error("/dev/snd/pcm Unsupported PCM sample depth of {}", m_pcmBitDepth);
        exit(1);
    }

    // ~100ms of audio per buffer
    samplesPerBuffer = m_pcmSampleRate / 10;
    for (int i = 0; i < AUDIOCONTEXT_NUM_SAMPLE_BUFFERS; i++) {
        sampleBuffers[i].data = new uint8_t[samplesPerBuffer * m_pcmChannels * (m_pcmBitDepth / 8)];
        sampleBuffers[i].samples = 0;
    }
}

float AudioContext::PlaybackProgress() const {
    if (!m_isDecoderRunning) {
        return 0;
    }

    return m_lastTimestamp;
}

void AudioContext::PlaybackStop() {
    if (m_isDecoderRunning) {
        m_isDecoderRunning = false;
        // Make the decoder stop waiting
        decoderWaitCondition.notify_all();

        m_decoderThread.join();
    }

    // Only change here as m_currentTrack is only used by the GUI thread
    m_currentTrack = nullptr;
}

void AudioContext::PlaybackSeek(float timestamp) {
    if (m_isDecoderRunning) {
        m_seekTimestamp = timestamp;
        m_requestSeek = true;

        // Let the decoder thread know that we want to seek
        decoderWaitCondition.notify_all();
        while(m_requestSeek) 
            usleep(10000); // Wait for seek to finish
    }
}

int AudioContext::PlayTrack(TrackInfo* info) {
    if (m_isDecoderRunning) {
        PlaybackStop();
    }

    std::lock_guard lockDecoder(m_decoderLock);

    assert(!m_avfmt);
    m_avfmt = avformat_alloc_context();
    if (int r = avformat_open_input(&m_avfmt, info->filepath.c_str(), NULL, NULL); r) {
        Lemon::Logger::Error("Failed to open {}", info->filepath);
        return r;
    }

    if (int r = avformat_find_stream_info(m_avfmt, NULL); r) {
        return r;
    }

    // Update track metadata in case the file has changed
    GetTrackInfo(m_avfmt, info);

    int streamIndex = av_find_best_stream(m_avfmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (streamIndex < 0) {
        Lemon::Logger::Error("Failed to get audio stream for {}", info->filepath);
        return streamIndex;
    }

    av_dump_format(m_avfmt, 0, info->filepath.c_str(), 0);

    m_currentStream = m_avfmt->streams[streamIndex];
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
    }

    m_currentTrack = info;

    m_isDecoderRunning = true;
    m_decoderThread = std::thread(DecodeAudio, this);
    return 0;
}

int AudioContext::LoadTrack(std::string filepath, TrackInfo* info) {
    AVFormatContext* fmt = avformat_alloc_context();
    if (int r = avformat_open_input(&fmt, filepath.c_str(), NULL, NULL); r) {
        Lemon::Logger::Error("Failed to open {}", filepath);
        return r;
    }

    if (int r = avformat_find_stream_info(fmt, NULL); r) {
        avformat_free_context(fmt);
        return r;
    }

    info->filepath = filepath;
    GetTrackInfo(fmt, info);

    avformat_free_context(fmt);
    return 0;
}

void AudioContext::GetTrackInfo(struct AVFormatContext* fmt, TrackInfo* info) {
    float durationSeconds = fmt->duration / 1000000.f;
    info->duration = durationSeconds;
    info->durationString = fmt::format("{:02}:{:02}", (int)durationSeconds / 60, (int)durationSeconds % 60);

    // Get file metadata
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
    }

    tag = av_dict_get(fmt->metadata, "album", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        info->metadata.album = tag->value;
    }

    tag = av_dict_get(fmt->metadata, "date", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag) {
        // First 4 digits are year
        info->metadata.year = std::string(tag->value).substr(4);
    }
}
