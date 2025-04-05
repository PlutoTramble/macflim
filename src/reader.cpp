#include "reader.hpp"



void ffmpeg_reader::init_reader(const std::string &movie_path, double &from, double &duration){
    av_log_set_level(AV_LOG_WARNING);
    if (avformat_open_input(&format_context_, movie_path.c_str(), NULL, NULL) != 0) {
        throw "Cannot open input file";
    }
    if (avformat_find_stream_info(format_context_, NULL) < 0) {
        throw "Cannot find stream information";
    }
    if (sDebug) {
        std::clog << "Searching for audio and video in " << format_context_->nb_streams << " streams\n";
    }

    ixv = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, &video_decoder_, 0);
    ixa = av_find_best_stream(format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_decoder_, 0);

    if (ixv == AVERROR_STREAM_NOT_FOUND) {
        throw "NO VIDEO IN FILE";
    }
    if (ixv == AVERROR_DECODER_NOT_FOUND) {
        throw "NO SUITABLE VIDEO DECODER AVAILABLE";
    }
    if (ixa == AVERROR_STREAM_NOT_FOUND) {
        std::cerr << "NO SOUND -- INSERTING SILENCE";
    }
    if (ixa == AVERROR_DECODER_NOT_FOUND) {
        throw "NO SUITABLE AUDIO DECODER AVAILABLE";
    }

    if (sDebug) {
        std::clog << "Video stream index :" << ixv << "\n";
        std::clog << "Audio stream index :" << ixa << "\n";
    }

    // TODO : May be removed -- maybe no longer needed?
    double actual_duration = format_context_->duration / (double)AV_TIME_BASE;
    if (duration > actual_duration) {
        std::clog << "Warning: Requested duration (" << duration << "s) exceeds video length ("
                  << actual_duration << "s). Trimming to video length.\n";
        duration = actual_duration;
    }

    double seek_to = std::max(from - 10.0, 0.0);    //  We seek to 10 seconds earlier, if we can
    if (avformat_seek_file(format_context_, -1, seek_to * AV_TIME_BASE,
                           seek_to * AV_TIME_BASE, seek_to * AV_TIME_BASE,
                           AVSEEK_FLAG_ANY) < 0) {
        throw "CANNOT SEEK IN FILE";
    }

    video_stream_ = format_context_->streams[ixv];
    audio_stream_ = ixa != AVERROR_STREAM_NOT_FOUND ? format_context_->streams[ixa] : nullptr;

    if (sDebug) {
        std::clog << "Video : " << video_stream_->codecpar->width << "x"
                  << video_stream_->codecpar->height << "@"
                  << av_q2d(video_stream_->r_frame_rate) << " fps"
                  << " timebase:" << av_q2d(video_stream_->time_base) << "\n";
        if (audio_stream_) {
            std::clog << "Audio : " << audio_stream_->codecpar->sample_rate << "Hz \n";
        }
    }

    first_frame_second_ = from;

    // TODO : May be removed -- maybe no longer needed?
    frame_to_extract_ = duration * av_q2d(video_stream_->r_frame_rate);
}

void ffmpeg_reader::init_video_context() {
    video_codec_context_ = avcodec_alloc_context3(video_decoder_);
    if (!video_codec_context_) {
        throw "CANNOT ALLOCATE VIDEO CODEC CONTEXT";
    }

    if (avcodec_parameters_to_context(video_codec_context_, video_stream_->codecpar) < 0) {
        throw "FAILED TO COPY VIDEO CODEC PARAMETERS";
    }

    AVDictionary *opts = NULL;
    av_dict_set(&opts, "refcounted_frames", "0", 0);    //  Do not refcount

    if (avcodec_open2(video_codec_context_, video_decoder_, &opts) < 0) {
        throw "CANNOT OPEN VIDEO CODEC";
    }

    if (sDebug) {
        std::clog << "VIDEO CODEC OPENED WITH PIXEL FORMAT "
                  << av_get_pix_fmt_name(video_codec_context_->pix_fmt) << "\n";
    }

    if (video_codec_context_->pix_fmt != AV_PIX_FMT_YUV420P) {
        throw "WAS EXPECTING A YUV420P PIXEL FORMAT";
    }
}

void ffmpeg_reader::init_audio_context() {
    if (ixa != AVERROR_STREAM_NOT_FOUND) {
        audio_codec_context_ = avcodec_alloc_context3(audio_decoder_);
        if (!audio_codec_context_) {
            throw "CANNOT ALLOCATE AUDIO CODEC CONTEXT";
        }

        if (avcodec_parameters_to_context(audio_codec_context_, audio_stream_->codecpar) < 0) {
            throw "FAILED TO COPY AUDIO CODEC PARAMETERS";
        }

        if (avcodec_open2(audio_codec_context_, audio_decoder_, nullptr) < 0) {
            throw "CANNOT OPEN AUDIO CODEC";
        }

        if (sDebug) {
            std::clog << "AUDIO CODEC: " << avcodec_get_name(audio_codec_context_->codec_id) << "\n";
        }

        AVSampleFormat sfmt = audio_codec_context_->sample_fmt;
        int n_channels = audio_codec_context_->ch_layout.nb_channels;

        if (sDebug) {
            std::clog << "SAMPLE FORMAT:" << av_get_sample_fmt_name(sfmt) << "\n";
            std::clog << "# CHANNELS   :" << n_channels << "\n";
            std::clog << "PLANAR       :" << (av_sample_fmt_is_planar(sfmt) ? "YES" : "NO") << "\n";
        }

        if (av_sample_fmt_is_planar(sfmt)) {
            sfmt = av_get_packed_sample_fmt(sfmt);

            if (sDebug) {
                std::clog << "PACKED FORMAT:" << av_get_sample_fmt_name(sfmt) << "\n";
                std::clog << "SAMPLE RATE  :" << audio_codec_context_->sample_rate << "\n";
            }
        }
        sound_ = std::make_unique<sound_buffer>(n_channels, audio_codec_context_->sample_rate);
    } else {
        sound_ = nullptr;
    }
}

image* ffmpeg_reader::decode_video(AVFrame* frame, AVPacket* pkt, AVFrame*& cloned_frame) {
    image* decoded_image_ptr = nullptr;

    if (avcodec_send_packet(video_codec_context_, pkt) == 0 && avcodec_receive_frame(video_codec_context_, frame) == 0) {
        cloned_frame = av_frame_clone(frame);
        if (cloned_frame->pts * av_q2d(video_stream_->time_base) >= first_frame_second_) { //&& images_.size() <= video_frame_count) {
            #ifdef VERBOSE
            printf("video_frame%s n:%d coded_n:%d presentation_ts:%ld / %f\n",
                    video_frame_count, frame_->pts,
                    cloned_frame->pts * av_q2d(video_stream_->time_base));
            #endif
            video_frame_count++;
            std::clog << "Read " << video_frame_count << " frames\r" << std::flush;

            av_image_copy(
                    video_dst_data_, video_dst_linesize_,
                    (const uint8_t **)(cloned_frame->data), cloned_frame->linesize,
                    video_codec_context_->pix_fmt, video_codec_context_->width,
                    video_codec_context_->height);

            video_image_->set_luma(video_dst_data_[0]);

            images_.push_back(*default_image_);
            copy(images_.back(), *video_image_);
            decoded_image_ptr = new image(images_.front());
            images_.pop_front();
        }
        #ifdef VERBOSE
        else {
            std::clog << "." << std::flush;  //  We are skipping frames
        }
        #endif
    }
    return decoded_image_ptr;
}