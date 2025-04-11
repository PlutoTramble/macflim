#ifndef READER_INCLUDED__
#define READER_INCLUDED__

#include <string>
#include <memory>
#include <array>
#include <iostream>
#include <algorithm>
#include <deque>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/timestamp.h>
    #include <libavcodec/avcodec.h>
}

#include "image.hpp"

/// A macintosh formatted sound frame (370 bytes)
class sound_frame_t
{
    public:
    static const size_t size = 370;

    protected:
    std::array<uint8_t,size> data_;

    public:
        sound_frame_t()
        {
            for (int i=0;i!=size;i++)
                data_[i] = 128;
        }

        u_int8_t &at( size_t i ) { return data_[i]; }

        std::array<uint8_t,size>::const_iterator begin() const { return std::cbegin(data_); }
        std::array<uint8_t,size>::const_iterator end() const { return std::cend(data_); }
};

class input_reader
{
public:
    virtual ~input_reader() {}

        //  Frame rate of the returned images
    virtual double frame_rate() = 0;

        //  Return extract_video_frame image until no more images are available
    virtual image* extract_video_frame() = 0;
    // virtual std::vector<image> images() = 0;

        //  Get the extract_video_frame sound sample, mac format
    virtual sound_frame_t* extract_sound_frame() = 0;
};






inline size_t ticks_from_frame( size_t n, double fps ) { return n/fps*60+.5; }
/*
 * A filesystem reader can read 512x342 8 bits pgm files numbered from 1. Audio has to be raw 8 bits unsigned.
 */
class filesystem_reader : public input_reader
{
    std::string file_pattern_;
    double frame_rate_;
    std::string audio_path_;
    size_t from_frame_;
    size_t count_;

    size_t current_image_index_;
    bool image_read_ = false;

    size_t frame_from_image( size_t n ) const
    {
        return ticks_from_frame( n-1, frame_rate_ );
    }

    public:
        filesystem_reader( const std::string &file_pattern, double frame_rate, const std::string &audio_path, size_t from_frame, size_t count ) :
            file_pattern_{file_pattern},
            frame_rate_{frame_rate},
            audio_path_{audio_path},
            from_frame_{from_frame},
            count_{count}
            {
                current_image_index_ = from_frame_;
            }

    virtual double frame_rate() { return frame_rate_; }

    virtual image* extract_video_frame(){} // TODO : Fix this
//    {
//        auto img = std::make_unique<image>( 512, 342 ); //  'cause read_image don't support anything else for now
//
//        if (image_read_)
//            return nullptr;
//
//        if (current_image_index_>=from_frame_+count_)
//        {
//            image_read_ = true;
//            return nullptr;
//        }
//
//        char buffer[1024];
//        sprintf( buffer, file_pattern_.c_str(), current_image_index_ );
//
//        if (!read_image( *(img.get()), buffer ))
//        {
//            image_read_ = true;
//            return nullptr;
//        }
//
//        current_image_index_++;
//
//        std::clog << "." << std::flush;
//
//        return img;
//    }

/*
    virtual size_t sample_rate() { return 370*60; };

    virtual std::vector<double> raw_sound()
    {
        std::vector<uint8_t> audio_samples;

        long audio_start = frame_from_image(from_frame_)*370;    //  Images are one-based
        long audio_end = frame_from_image(from_frame_+count_)*370;      //  Last image is included?
        long audio_size = audio_end-audio_start;

        FILE *f = fopen( audio_path_.c_str(), "rb" );
        if (f)
        {
            audio_samples.resize( audio_size );
            for (auto &v:audio_samples)
                v = 0x80;

            fseek( f, audio_start, SEEK_SET );
            auto res = fread( audio_samples.data(), 1, audio_size, f );
            if (res!=audio_size)
                std::clog << "AUDIO: added " << audio_size-res << " bytes of silence\n";
            fclose( f );
        }
        else
            std::cerr << "**** ERROR: CANNOT OPEN AUDIO FILE [" << audio_path_ << "]\n";
        std::clog << "AUDIO: READ " << audio_size << " bytes from offset " << audio_start << "\n";

        auto min_sample = *std::min_element( std::begin(audio_samples), std::end(audio_samples) );
        auto max_sample = *std::max_element( std::begin(audio_samples), std::end(audio_samples) );
        std::clog << " SAMPLE MIN:" << (int)min_sample << " SAMPLE MAX:" << (int)max_sample << "\n";

        std::vector<double> res;
        for (auto s:audio_samples)
            res.push_back( (s-128.0)/128.0 );
        
        return res;
    }
*/
    virtual sound_frame_t* extract_sound_frame() { return nullptr; }         //  #### THIS IS COMPLETELY WRONG

};


extern bool sDebug;

/// This stores a sound buffer and transform it into a suitable format for flims
class sound_buffer {
    std::deque<float> data_;
    size_t channel_count_ = 0;      //  # of channels
    size_t sample_rate_ = 0;        //  # of samples per second
    float min_sample_;
    float max_sample_;

    size_t sound_frame_size() {
        return static_cast<size_t>(std::round(sample_rate_ / 60.0));
    }

public:
    sound_buffer(size_t channel_count, size_t sample_rate) :
            channel_count_{channel_count},
            sample_rate_{sample_rate}
    {}

    void append_silence(float duration) {
        size_t sample_count = sample_rate_ * duration;
        for (size_t i = 0; i != sample_count; i++) {
            data_.push_back(0);
        }
    }

    void append_samples(float **samples, size_t sample_count) {
        for (size_t i = 0; i != sample_count; i++) {
            float v = 0;
            for (size_t j = 0; j != channel_count_; j++) {
                v += samples[j][i];
            }
            data_.push_back(v / channel_count_);
        }
    }

    bool isEmpty() {return data_.empty();}

    size_t sound_frames_contained() {
        return data_.size() / sound_frame_size();
    }

    sound_frame_t& extract_front() {
        size_t samples_needed = sound_frame_size();

        auto* s_frame = new sound_frame_t();

        if (data_.empty()) {
            return *s_frame;
        }

        size_t available_samples = std::min(samples_needed, data_.size());

        // Normalization
        min_sample_ = *std::min_element(data_.begin(), data_.begin() + available_samples);
        max_sample_ = *std::max_element(data_.begin(), data_.begin() + available_samples);

        if (min_sample_ == max_sample_) {
            min_sample_ -= 1.0f;
            max_sample_ += 1.0f;
        }

        for (int i = 0; i != sound_frame_t::size; i++) {
            float alpha = static_cast<float>(i) / sound_frame_t::size;
            size_t sample_index = static_cast<size_t>(alpha * samples_needed);

            if (sample_index < available_samples) {
                float sample = data_[sample_index];
                uint8_t encoded = static_cast<uint8_t>(std::clamp((sample - min_sample_) / (max_sample_ - min_sample_) * 255.0f,0.0f, 255.0f));
                s_frame->at(i) = encoded;
            } else {
                s_frame->at(i) = 128; // silence for missing samples
            }
        }

        data_.erase(data_.begin(), data_.begin() + available_samples);
        return *s_frame;
    }
};

class ffmpeg_reader : public input_reader {
    AVFormatContext *format_context_ = nullptr;
    const AVCodec *video_decoder_;
    const AVCodec *audio_decoder_;
    AVStream *video_stream_;
    AVStream *audio_stream_;
    AVCodecContext *video_codec_context_;
    AVCodecContext *audio_codec_context_;
    uint8_t *video_dst_data_[4] = {NULL};
    int video_dst_linesize_[4];
    int ixv;    //  Video frame index
    int ixa;    //  Audio frame index
    size_t video_frame_count = 0;
    std::unique_ptr<image> video_image_;        //  Size of the video input
    std::unique_ptr<image> default_image_;      //  Size of our output
    std::deque<image> images_;                  //  Image read buffer
    std::unique_ptr<sound_buffer> sound_;
    int image_ix = -1;
    int sound_ix = -1;
    double first_frame_second_;
    size_t frames_to_extract_;
    size_t extracted_frames_ = 0;
    bool found_sound_ = false;                   //  To track if sounds starts with an offset


    void init_video_context();

    void init_audio_context();

    void init_reader(const std::string &movie_path, double &from, double &duration);

    void read() {
        auto bufsize = av_image_alloc(
                video_dst_data_,
                video_dst_linesize_,
                video_codec_context_->width,
                video_codec_context_->height,
                video_codec_context_->pix_fmt,
                1);

        if (bufsize < 0) {
            throw "CANNOT ALLOCATE IMAGE";
        }

        video_image_ = std::make_unique<image>(video_codec_context_->width, video_codec_context_->height);

        double aspect = video_codec_context_->width / (double)video_codec_context_->height;
        if (aspect > 512 / 342.0) {
            default_image_ = std::make_unique<image>(342 * aspect, 342);
        } else {
            default_image_ = std::make_unique<image>(512, 512 / aspect);
        }

        if (sDebug) {
            std::clog << "Image structure:\n";
            std::clog << video_dst_linesize_[0] << " " << video_dst_linesize_[1] << " "
                      << video_dst_linesize_[2] << " " << video_dst_linesize_[3] << "\n";
            std::clog << (video_dst_linesize_[0] + video_dst_linesize_[1] +
                          video_dst_linesize_[2] + video_dst_linesize_[3]) *
                         video_codec_context_->height << "\n";
            std::clog << bufsize << "\n";
        }
    }

public:
    ffmpeg_reader() {}

    ffmpeg_reader(const std::string &movie_path, double from, double duration) {

        init_reader(movie_path, from, duration);

        init_video_context();
        init_audio_context();

        read();
    }

    ~ffmpeg_reader() {
        avformat_close_input(&format_context_);
        avcodec_free_context(&video_codec_context_);
        if (audio_codec_context_) {
            avcodec_free_context(&audio_codec_context_);
        }
        av_freep(&video_dst_data_[0]);
        if (sDebug) {
            std::clog << "Closed media file\n";
        }
    }

    AVFormatContext* get_format_context() const {
        return this->format_context_;
    }

    int get_video_frame_index() const {return ixv;}
    int get_audio_frame_index() const {return ixa;}
    size_t get_frames_to_extract() const {return frames_to_extract_;}
    size_t get_read_images() const {return video_frame_count;}
    size_t get_extracted_frames() const {return extracted_frames_;}

    bool can_extract_frames(size_t num_of_ticks) {
        if(!images_.empty() && (frames_to_extract_ - extracted_frames_) < 2) {
            // No need to check for sound when we're at the last frames
            return true;
        }
        else if (images_.empty() || sound_->sound_frames_contained() < num_of_ticks) {
            // Buffer not ready for extraction
            return false;
        }

        return true;
    }

    void decode_video(AVFrame* frame, AVPacket* pkt, AVFrame*& cloned_frame);
    void decode_sound(AVFrame* frame, AVPacket* pkt, AVFrame*& cloned_frame);

    virtual double frame_rate() {
        return av_q2d(video_stream_->r_frame_rate);
    }

    virtual image* extract_video_frame() {
        if(images_.empty()) {
            return nullptr;
        }

        image* image_ptr = new image(images_.front());
        images_.pop_front();
        extracted_frames_++;

        return image_ptr;
    }

    virtual sound_frame_t* extract_sound_frame() {
        if (ixa != AVERROR_STREAM_NOT_FOUND) {
            return &sound_->extract_front();
        }
        return nullptr;
    }
};

#endif
