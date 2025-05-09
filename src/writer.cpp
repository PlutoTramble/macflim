#include "writer.hpp"

#include <iostream>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/time.h>
    #include <libavutil/opt.h>
}

extern bool sDebug;

class ffmpeg_writer : public output_writer {
    size_t W_; // Should be in output_writer
    size_t H_;

    /* check that a given sample format is supported by the encoder */
    static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt) {
        const enum AVSampleFormat *p = codec->sample_fmts;
        while (*p != AV_SAMPLE_FMT_NONE) {
            if (sDebug)
                std::clog << "[ " << av_get_sample_fmt_name(*p) << "] ";
            if (*p == sample_fmt)
                return 1;
            p++;
        }
        return 0;
    }

    AVFrame *videoFrame = nullptr;
    AVFrame *audio_frame = nullptr;
    AVCodecContext* video_context = nullptr;
    AVCodecContext* audio_context = nullptr;
    int frameCounter = 0;
    AVFormatContext* ofctx = nullptr;
    const AVOutputFormat* oformat = nullptr;

    // Small state for audio encoding (22100 in 370 u8 sample to 44200 in 1024 flt samples)
    float audio_44[735];
    size_t audio_pos = 0;
    int audio_frame_counter = 0;

    void pushFrame(const image &img, const sound_frame_t &snd) {
        int err;
        if (!videoFrame) {
            videoFrame = av_frame_alloc();
            if (!videoFrame) {
                throw "Failed to allocate video frame";
            }
            videoFrame->format = AV_PIX_FMT_YUV420P;
            videoFrame->width = video_context->width;
            videoFrame->height = video_context->height;
            if ((err = av_frame_get_buffer(videoFrame, 32)) < 0) {
                std::cout << "Failed to allocate picture buffer: " << err << std::endl;
                return;
            }
            av_frame_make_writable(videoFrame);

            memset(videoFrame->data[1], 128, H_/2 * videoFrame->linesize[1]);
            memset(videoFrame->data[2], 128, H_/2 * videoFrame->linesize[2]);
        }

        uint8_t *p = videoFrame->data[0];
        for (size_t y = 0; y != H_; y++) {
            for (size_t x = 0; x != W_; x++) {
                auto v = img.at(x, y);
                *p++ = v <= 0.5 ? 0 : 255;
            }
        }

        videoFrame->pts = 1500 * frameCounter;

        if ((err = avcodec_send_frame(video_context, videoFrame)) < 0) {
            std::cout << "Failed to send frame: " << err << std::endl;
            return;
        }

        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        pkt.flags |= AV_PKT_FLAG_KEY;
        if (avcodec_receive_packet(video_context, &pkt) == 0) {
            av_interleaved_write_frame(ofctx, &pkt);
            av_packet_unref(&pkt);
        }

        float *audio_p = (float *)audio_frame->data[0];

        // AUDIO
        // We convert to 44KHz
        for (int i = 0; i != 735; i++) {
            audio_44[i] = (*(snd.begin() + (int)(i / 735.0 * 370)) - 128.0) / 128;
        }

        // How many samples left to send?
        if (audio_pos + 735 >= 1024) {
            memcpy(audio_p + audio_pos, audio_44, (1024 - audio_pos) * sizeof(float));

            audio_frame->pts = audio_frame_counter * 1024;
            audio_frame_counter++;

            err = avcodec_send_frame(audio_context, audio_frame);
            if (err < 0)
                throw "Error sending the frame to the encoder";

            err = avcodec_receive_packet(audio_context, &pkt);
            if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
                return;
            else if (err < 0)
                throw "Error encoding audio frame";

            pkt.stream_index = 1; // Corrected this line

            av_interleaved_write_frame(ofctx, &pkt);
            av_packet_unref(&pkt);

            memcpy(audio_p, audio_44 + 1024 - audio_pos, (735 - 1024 + audio_pos) * sizeof(float));
            audio_pos = 735 - 1024 + audio_pos;
        } else {
            memcpy(audio_p + audio_pos, audio_44, 735 * sizeof(float));
            audio_pos += 735;
        }

        frameCounter++;
    }

    static void dump_codecs() {
        void *i = 0;
        const AVCodec *p;

        while ((p = av_codec_iterate(&i))) {
            std::clog << p->name << " ";
        }

        std::clog << "\n";
    }

public:
    ffmpeg_writer(const std::string filename, size_t W, size_t H) {
        W_ = W;
        H_ = H;

        oformat = av_guess_format(nullptr, filename.c_str(), nullptr);
        if (!oformat) {
            throw "Can't create output format";
        }

        int err = avformat_alloc_output_context2(&ofctx, oformat, nullptr, filename.c_str());
        if (err < 0) {
            throw "can't create output context";
        }

        const AVCodec* video_codec = nullptr;
        video_codec = avcodec_find_encoder(oformat->video_codec);
        if (!video_codec) {
            throw "Can't create video codec";
        }

        AVStream* stream = avformat_new_stream(ofctx, video_codec);
        if (!stream) {
            std::cout << "can't find format" << std::endl;
        }

        video_context = avcodec_alloc_context3(video_codec);

        if (!video_context) {
            throw "Can't create video codec context";
        }

        stream->codecpar->codec_id = oformat->video_codec;
        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        stream->codecpar->width = W_;
        stream->codecpar->height = H_;
        stream->codecpar->format = AV_PIX_FMT_YUV420P;
        stream->codecpar->bit_rate = 60 * 6000 * 8;

        int ret = avcodec_parameters_to_context(video_context, stream->codecpar);
        if (ret < 0) {
            throw "Failed to copy codec parameters to context";
        }

        video_context->time_base = (AVRational){ 1, 30 };
        video_context->max_b_frames = 2;
        video_context->gop_size = 12;
        video_context->framerate = (AVRational){ 60, 1 };

        if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
            av_opt_set(video_context, "preset", "ultrafast", 0);
        } else if (stream->codecpar->codec_id == AV_CODEC_ID_H265) {
            av_opt_set(video_context, "preset", "ultrafast", 0);
        }

        avcodec_parameters_from_context(stream->codecpar, video_context);

        if ((err = avcodec_open2(video_context, video_codec, NULL)) < 0) {
            throw "Failed to open codec";
        }

        // AUDIO
        auto audio_codec = avcodec_find_encoder(oformat->audio_codec);
        if (!audio_codec)
            throw "Audio codec not found";

        audio_context = avcodec_alloc_context3(audio_codec);
        if (!audio_context)
            throw "Could not allocate audio codec context";

        audio_context->sample_fmt = AV_SAMPLE_FMT_FLTP;
        if (!check_sample_fmt(audio_codec, audio_context->sample_fmt))
            throw "Encoder does not support FLT planar samples";

        audio_context->sample_rate = 44100;
        audio_context->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
        audio_context->ch_layout.nb_channels = 1;

        ret = avcodec_open2(audio_context, audio_codec, NULL);
        if (ret < 0) {
            throw "Could not open audio codec";
        }

        AVStream* audio_stream = avformat_new_stream(ofctx, audio_codec);
        if (!audio_stream) {
            throw "Cannot create audio stream";
        }

        audio_stream->id = 1;

        audio_stream->time_base = (AVRational){ 1, 44100 };
        avcodec_parameters_from_context(audio_stream->codecpar, audio_context);

        audio_frame = av_frame_alloc();
        if (!audio_frame)
            throw "Error allocating an audio frame";

        audio_frame->format = audio_context->sample_fmt;
        audio_frame->ch_layout = audio_context->ch_layout;
        audio_frame->sample_rate = audio_context->sample_rate;
        audio_frame->nb_samples = 1024; // 44100/60
        err = av_frame_get_buffer(audio_frame, 0);
        if (err < 0) {
            throw "Error allocating an audio buffer";
        }

        if (sDebug) {
            std::clog << "Line size = " << audio_frame->linesize[0] << "\n";
            std::clog << "Frame size = " << audio_context->frame_size << "\n";
        }

        if (!(oformat->flags & AVFMT_NOFILE)) {
            if ((err = avio_open(&ofctx->pb, filename.c_str(), AVIO_FLAG_WRITE)) < 0) {
                throw "Failed to open file";
            }
        }

        if ((err = avformat_write_header(ofctx, NULL)) < 0) {
            char buffer[1025];
            av_strerror(err, buffer, 1024);
            std::clog << err << ": " << buffer << "\n";
            throw "Failed to write header";
        }

        av_dump_format(ofctx, 0, filename.c_str(), 1);
    }

    ~ffmpeg_writer() {
        if (sDebug)
            std::clog << "~ffmpeg_writer()\n";

        // DELAYED FRAMES
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;

        for (;;) {
            avcodec_send_frame(video_context, NULL);
            if (avcodec_receive_packet(video_context, &pkt) == 0) {
                av_interleaved_write_frame(ofctx, &pkt);
                av_packet_unref(&pkt);
            } else {
                break;
            }
        }

        av_write_trailer(ofctx);
        if (!(oformat->flags & AVFMT_NOFILE)) {
            int err = avio_close(ofctx->pb);
            if (err < 0) {
                std::cout << "Failed to close file: " << err << std::endl;
            }
        }

        if (videoFrame) {
            av_frame_free(&videoFrame);
        }
        if (audio_frame) {
            av_frame_free(&audio_frame);
        }
        if (video_context) {
            avcodec_free_context(&video_context);
        }
        if (audio_context) {
            avcodec_free_context(&audio_context);
        }
        if (ofctx) {
            avformat_free_context(ofctx);
        }

        if (sDebug)
            std::clog << "#### End of video stream\n";
    }

    virtual void write_frame(const image& img, const sound_frame_t &snd) {
        pushFrame(img, snd);
    }
};

class gif_writer : public output_writer {
    size_t count_ = 0;
    size_t num_ = 0;
    std::string filename_;

public:
    gif_writer(const std::string filename) : filename_{ filename } {}

    virtual void write_frame(const image& img, [[maybe_unused]] const sound_frame_t &snd) {
        if ((count_ % 3) == 0) {
            char buffer[1024];
            sprintf(buffer, "/tmp/gif-%06lu.pgm", num_);
            write_image(buffer, img);
            num_++;
        }
        count_++;
    }

    ~gif_writer() {
        char buffer[1024];
        sprintf(buffer, "convert -delay 5 -loop 0 /tmp/gif-*.pgm '%s'", filename_.c_str());
        std::clog << "GENERATING GIF FILE\n";
        int res = system(buffer);
        if (res != 0) {
            std::cerr << "**** FAILED TO GENERATE GIF FILE (retcode=" << res << ")\n";
        }
        std::clog << "DONE\n";
        delete_files_of_pattern("/tmp/gif-%06d.pgm");
    }
};

std::unique_ptr<output_writer> make_ffmpeg_writer(const std::string &movie_path, size_t w, size_t h) {
    return std::make_unique<ffmpeg_writer>(movie_path, w, h);
}

std::unique_ptr<output_writer> make_gif_writer(const std::string &movie_path, [[maybe_unused]] size_t w, [[maybe_unused]] size_t h) {
    return std::make_unique<gif_writer>(movie_path);
}

class null_writer : public output_writer {
public:
    virtual void write_frame([[maybe_unused]] const image& img, [[maybe_unused]] const sound_frame_t &snd) {}
};

std::unique_ptr<output_writer> make_null_writer() {
    return std::make_unique<null_writer>();
}