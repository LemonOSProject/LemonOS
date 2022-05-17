#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <sys/ioctl.h>

#include <string>

#include <Lemon/System/ABI/Audio.h>

struct SampleBuffer {
    uint8_t* data = nullptr;
    long samples = 0;
};
ssize_t maxSamplesInBuffer;

// PCM output file descriptor
int pcm;
int outputEncoding;
int outputSampleSize = 2;

void WriteSamples(SampleBuffer* buffer) {
    write(pcm, buffer->data, buffer->samples * 2 * outputSampleSize);
    buffer->samples = 0;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: mp3play <file>\n");
        return EINVAL;
    }

    pcm = open("/dev/snd/pcm", O_WRONLY);
    if (pcm == -1) {
        perror("open /dev/snd/pcm: ");
        return errno;
    }

    // Get output information
    int numChannels = ioctl(pcm, IoCtlOutputGetNumberOfChannels);
    if (numChannels < 0) {
        perror("/dev/snd/pcm IoCtlOutputGetNumberOfChannels:");
        return errno;
    }
    outputEncoding = ioctl(pcm, IoCtlOutputGetEncoding);
    printf("out enc %d\n", ioctl(pcm, IoCtlOutputGetEncoding));
    if (outputEncoding < 0) {
        perror("/dev/snd/pcm IoCtlOutputGetEncoding:");
        return errno;
    }
    int sampleRate = ioctl(pcm, IoCtlOutputGetSampleRate);
    if (sampleRate < 0) {
        perror("/dev/snd/pcm IoCtlOutputGetSampleRate:");
        return errno;
    }

    const char* encodingString = "Unknown Encoding";
    if (outputEncoding >= 0 && outputEncoding < LEMON_ABI_AUDIO_ENCODING_COUNT) {
        encodingString = lemonABIAudioEncodingNames[outputEncoding];
    }

    assert(outputEncoding == SoundEncoding::PCMS16LE);

    if(outputEncoding == PCMS16LE) {
        outputSampleSize = 2;
    } else if(outputEncoding == PCMS20LE) {
        // 20-bit samples take up a dword
        outputSampleSize = 4;
    }

    printf("[/dev/snd/pcm] Channels: %d, Encoding: %s, Sample Rate: %dHz\n", numChannels, encodingString, sampleRate);

    AVFormatContext* fctx = avformat_alloc_context();
    if (avformat_open_input(&fctx, argv[1], NULL, NULL)) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        return 1;
    }

    if (avformat_find_stream_info(fctx, NULL)) {
        fprintf(stderr, "Failed to get stream info, potentialy corrupted\n");
        return 1;
    }

    std::string title = argv[1];
    std::string artist = "Unknown Artist";

    // Get file metadata
    AVDictionaryEntry *tag = nullptr;
    tag = av_dict_get(fctx->metadata, "title", tag, AV_DICT_IGNORE_SUFFIX);
    if(tag) {
        title = tag->value;
    }

    tag = av_dict_get(fctx->metadata, "artist", tag, AV_DICT_IGNORE_SUFFIX);
    if(tag) {
        artist = tag->value;
    }

    long durationSeconds = fctx->duration / 1000000;
    printf("Playing %s - %s (%ld:%ld)\n", title.c_str(), artist.c_str(), durationSeconds / 60, durationSeconds % 60);

    int streamIndex = av_find_best_stream(fctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    assert(streamIndex >= 0);

    AVStream* stream = fctx->streams[streamIndex];

    av_dump_format(fctx, 0, argv[1], 0);

    AVPacket* packet = av_packet_alloc();
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Failed to find MP3 codec\n");
        return 1;
    }

    AVCodecContext* cctx = avcodec_alloc_context3(codec);
    assert(cctx);
    if (avcodec_parameters_to_context(cctx, stream->codecpar)) {
        fprintf(stderr, "Failed to init codec context\n");
        return 1;
    }

    if (avcodec_open2(cctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return 1;
    }

    SwrContext* sctx = swr_alloc();
    //av_opt_set_int(sctx, "in_channel_layout", codec->channel_layouts[0], 0);
    av_opt_set_int(sctx, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(sctx, "in_sample_rate", cctx->sample_rate, 0);
    av_opt_set_sample_fmt(sctx, "in_sample_fmt", cctx->sample_fmt, 0);

    // Dual channel
    av_opt_set_int(sctx, "out_channel_layout", /*AV_CH_LAYOUT_STEREO*/AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(sctx, "out_sample_rate", sampleRate, 0);
    // Output is signed 16-bit PCM packed

    if(outputEncoding == SoundEncoding::PCMS16LE) {
        av_opt_set_sample_fmt(sctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    } else {
        av_opt_set_sample_fmt(sctx, "out_sample_fmt", AV_SAMPLE_FMT_S32, 0);
    }

    if (swr_init(sctx)) {
        fprintf(stderr, "Could not initialize software resampler\n");
        return 1;
    }
    
    SampleBuffer sampleBuffer;

    // 1 second of audio
    maxSamplesInBuffer = sampleRate;
    sampleBuffer.data = new uint8_t[outputSampleSize * maxSamplesInBuffer * numChannels];
    sampleBuffer.samples = 0;

    AVFrame* frame = av_frame_alloc();
    while (av_read_frame(fctx, packet) >= 0) {
        if (avcodec_send_packet(cctx, packet)) {
            fprintf(stderr, "could not send packet for decoding\n");
            return 1;
        }

        ssize_t ret = 0;

        while (ret >= 0) {
            ret = avcodec_receive_frame(cctx, frame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                break;
            } else if (ret) {
                fprintf(stderr, "could not decode frame %zd\n", ret);
                return 1;
            }

            // Ensure buffer isnt being played
            int samplesToWrite = av_rescale_rnd(swr_get_delay(sctx, cctx->sample_rate) + frame->nb_samples, sampleRate, cctx->sample_rate, AV_ROUND_UP);
            if (sampleBuffer.samples + samplesToWrite + 32 > maxSamplesInBuffer) {
                WriteSamples(&sampleBuffer);
            }

            /* Basic resampling used previously
            float sampleRateFactor = sampleRate / cctx->sample_rate;
            int samplesToWrite = frame->nb_samples * sampleRateFactor;
            float** frameData = (float**)frame->extended_data;
            if(frame->channels >= 2) {
                for(int i = 0; i < samplesToWrite; i++) {
                    int sourceIndex = i // / sampleRateFactor;

                    float s1 = frameData[0][sourceIndex] * 32768;
                    float s2 = frameData[1][sourceIndex] * 32768;
                    if(s1 > 32767) s1 = 32767;
                    if(s1 < -32768) s1 = -32768;
                    if(s2 > 32767) s2 = 32767;
                    if(s2 < -32768) s2 = -32768;

                    sampleBuffer[(samplesInBuffer + i) * 2] = (short)s1;
                    sampleBuffer[(samplesInBuffer + i) * 2 + 1] = (short)s2;
                }
            }*/

            // libswresample resampling
            uint8_t* outputData = (uint8_t*)(sampleBuffer.data + sampleBuffer.samples * outputSampleSize * 2);
            ssize_t samplesWritten = swr_convert(sctx, &outputData, (int)(maxSamplesInBuffer - sampleBuffer.samples),
                                                 (const uint8_t**)frame->extended_data, frame->nb_samples);

            if (samplesWritten < 0) {
                printf("Resampling failed\n");
                return 1;
            }

            //printf("buf samples %ld, max buf samples %ld, frame samples %d, %ld samples written (expected %d)\n", buffer->samples, maxSamplesInBuffer, frame->nb_samples, samplesWritten, samplesToWrite);

            sampleBuffer.samples += samplesWritten;
            long timestampSeconds = frame->best_effort_timestamp / stream->time_base.den;
            printf("\r\e[K%02ld:%02ld/%02ld:%02ld", timestampSeconds / 60, timestampSeconds % 60, durationSeconds / 60, durationSeconds % 60);
            fflush(stdout);

            av_frame_unref(frame);
        }

        av_packet_unref(packet);
    }
    return 0;
}