#ifndef FLIMENCODER_INCLUDED__
#define FLIMENCODER_INCLUDED__

#include <string>

#include "flimcompressor.hpp"
#include "framegenerator.hpp"

#include "reader.hpp"
#include "writer.hpp"

#include <sstream>
#include <libavutil/frame.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>

extern bool sDebug;

/**
 * A set of encoding parameters
 */
class encoding_profile
{
protected:
    size_t W_ = 512;
    size_t H_ = 342;

    size_t byterate_ = 2000;
    double stability_ = 0.3;
    int fps_ratio_ = 1;
    bool group_ = true;
    std::string filters_ = "c";
    bool bars_ = true;              //  Do we put black bars around the image?

    image::dithering dither_ = image::error_diffusion;
    std::string error_algorithm_ = "floyd";
    float error_bleed_ = 1;
    bool error_bidi_ = false;

    bool silent_ = false;

    std::vector<flimcompressor::codec_spec> codecs_;

public:

    size_t width() const { return W_; }
    size_t height() const { return H_; }
    void set_size( size_t W, size_t H ) { W_ = W; H_ = H; }
    void set_width( size_t W ) { W_ = W; }
    void set_height( size_t H ) { H_ = H; }


    size_t byterate() const { return byterate_; }
    void set_byterate( size_t byterate ) { byterate_ = byterate; }

        //  Technically, we could put the half-rate/fps_ratio mecanism in the reader phase
        //  to avoid reading unecessary images, but it is more generic to put it here
        //  as it could allows to extend to dynamic half rate [yagni]
    int fps_ratio() const { return fps_ratio_; }
    void set_fps_ratio( int fps_ratio ) { fps_ratio_ = fps_ratio; }

    bool group() const { return group_; }
    void set_group( bool group ) { group_ = group; }

    std::string filters() const { return filters_; }
    void set_filters( const std::string filters ) { filters_ = filters; }

    bool bars() const { return bars_; }
    void set_bars( bool bars ) { bars_ = bars; }

    image::dithering dither() const { return dither_; }
    bool set_dither( std::string dither )
    {
        if (dither=="ordered")
            dither_ = image::ordered;
        else if (dither=="error")
            dither_ = image::error_diffusion;
        else
            throw "Wrong dither option : only 'ordered' and 'error' are supported";
        return true;
    }
    void set_dither( image::dithering dither ) { dither_ = dither; }

    std::string error_algorithm() const { return error_algorithm_; }
    void set_error_algorithm( const std::string algo ) { error_algorithm_ = algo; }

    float error_bleed() const { return error_bleed_; }
    void set_error_bleed( float bleed ) { error_bleed_ = bleed; }

    bool error_bidi() const { return error_bidi_; }
    void set_error_bidi( bool error_bidi ) { error_bidi_ = error_bidi; }

    double stability() const { return stability_; }
    void set_stability( double stability ) { stability_ = stability; }

    const std::vector<flimcompressor::codec_spec> &codecs() const { return codecs_; }
    void set_codecs( const std::vector<flimcompressor::codec_spec> &codecs ) { codecs_ = codecs; }

    bool silent() const { return silent_; }
    void set_silent( bool silent ) { silent_ = silent; }

    static bool profile_named( const std::string name, size_t width, size_t height, encoding_profile &result )
    {
        result.set_size( width, height );
        if (name=="128k"s)
        {
            result.set_byterate( 380 );
            result.set_filters( "g1.6bbscz" );
            result.set_fps_ratio( 4 );
            result.set_group( false );
            result.set_stability( 0.5 );
            result.set_bars( true );
            result.set_dither( "ordered" );
            result.set_error_algorithm( "floyd" );
            result.set_error_bidi( true );
            result.set_error_bleed( 0.95 );
            result.codecs_.clear();
            result.codecs_.push_back( flimcompressor::make_codec( "null", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "z32", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "lines:count=10", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "invert", result.W_, result.H_ ) );
            result.set_silent( true );
            return true;
        }
        if (name=="512k"s)
        {
            result.set_byterate( 480 );
            result.set_filters( "g1.6bbscz" );
            result.set_fps_ratio( 4 );
            result.set_group( false );
            result.set_stability( 0.5 );
            result.set_bars( true );
            result.set_dither( "ordered" );
            result.set_error_algorithm( "floyd" );
            result.set_error_bidi( true );
            result.set_error_bleed( 0.95 );
            result.codecs_.clear();
            result.codecs_.push_back( flimcompressor::make_codec( "null", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "z32", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "lines:count=10", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "invert", result.W_, result.H_ ) );
            result.set_silent( true );
            return true;
        }
        if (name=="xl"s)
        {
            result.set_byterate( 580 );
            result.set_filters( "g1.6bbsc" );
            result.set_fps_ratio( 4 );
            result.set_group( true );
            result.set_stability( 0.5 );
            result.set_bars( true );
            result.set_dither( "ordered" );
            result.set_error_algorithm( "floyd" );
            result.set_error_bidi( true );
            result.set_error_bleed( 0.95 );
            result.codecs_.clear();
            result.codecs_.push_back( flimcompressor::make_codec( "null", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "z32", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "lines:count=10", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "invert", result.W_, result.H_ ) );
            result.set_silent( true );
            return true;
        }
        if (name=="plus"s)
        {
            result.set_byterate( 1500 );
            result.set_filters( "g1.6bbscz" );
            result.set_fps_ratio( 2 );
            result.set_group( false );
            result.set_stability( 0.5 );
            result.set_bars( true );
            result.set_dither( "ordered" );
            result.set_error_algorithm( "floyd" );
            result.set_error_bidi( true );
            result.set_error_bleed( 0.95 );
            result.codecs_.clear();
            result.codecs_.push_back( flimcompressor::make_codec( "null", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "z32", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "lines:count=30", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "invert", result.W_, result.H_ ) );
            result.set_silent( false );
            return true;
        }
        if (name=="portable"s)
        {
            result.set_byterate( 2500 );
            result.set_filters( "g1.6bsc" );
            result.set_fps_ratio( 2 );
            result.set_group( false );
            result.set_stability( 0.5 );
            result.set_bars( true );
            result.set_dither( "error" );
            result.set_error_algorithm( "floyd" );
            result.set_error_bidi( true );
            result.set_error_bleed( 0.98 );
            result.codecs_.clear();
            result.codecs_.push_back( flimcompressor::make_codec( "null", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "z32", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "lines:count=50", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "invert", result.W_, result.H_ ) );
            result.set_silent( false );
            return true;
        }
        if (name=="se"s)
        {
            result.set_byterate( 2500 );
            result.set_filters( "g1.6bsc" );
            result.set_fps_ratio( 2 );
            result.set_group( false );
            result.set_stability( 0.5 );
            result.set_bars( true );
            result.set_dither( "error" );
            result.set_error_algorithm( "floyd" );
            result.set_error_bidi( true );
            result.set_error_bleed( 0.98 );
            result.codecs_.clear();
            result.codecs_.push_back( flimcompressor::make_codec( "null", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "z32", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "lines:count=50", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "invert", result.W_, result.H_ ) );
            result.set_silent( false );
            return true;
        }
        if (name=="se30"s)
        {
            result.set_byterate( 6000 );
            result.set_filters( "g1.6sc" );
            result.set_fps_ratio( 1 );
            result.set_group( true );
            result.set_stability( 0.3 );
            result.set_bars( false );
            result.set_dither( "error" );
            result.set_error_algorithm( "floyd" );
            result.set_error_bidi( true );
            result.set_error_bleed( 0.99 );
            result.codecs_.clear();
            result.codecs_.push_back( flimcompressor::make_codec( "null", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "z32", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "lines:count=70", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "invert", result.W_, result.H_ ) );
            result.set_silent( false );
            return true;
        }
        if (name=="perfect"s)
        {
            result.set_byterate( 32000 );
            result.set_filters( "g1.6sc" );
            result.set_fps_ratio( 1 );
            result.set_group( true );
            result.set_stability( 0.3 );
            result.set_bars( false );
            result.set_dither( "error" );
            result.set_error_algorithm( "floyd" );
            result.set_error_bidi( true );
            result.set_error_bleed( 1 );
            result.codecs_.clear();
            result.codecs_.push_back( flimcompressor::make_codec( "null", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "z32", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "lines:count=342", result.W_, result.H_ ) );
            result.codecs_.push_back( flimcompressor::make_codec( "invert", result.W_, result.H_ ) );
            result.set_silent( false );
            return true;
        }

        return false;
    }

    std::string dither_string() const
    {
        switch (dither_)
        {
            case image::error_diffusion:
                return "error";
            case image::ordered:
                return "ordered";
        }
        return "???";
    }

    std::string description() const
    {
        std::ostringstream cmd;

        cmd << "--byterate " << byterate_;
        cmd << " --fps-ratio " << fps_ratio_;
        cmd << " --group " << (group_?"true":"false");
        cmd << " --bars " << (bars_?"true":"false");
        cmd << " --dither " << dither_string();
        if (dither_==image::error_diffusion)
        {
            cmd << " --error-stability " << stability_;
            cmd << " --error-algorithm " << error_algorithm_;
            cmd << " --error-bidi " << error_bidi_;
            cmd << " --error-bleed " << error_bleed_;
        }
        cmd << " --filters " << filters_;

        for (auto &c:codecs_)
            cmd << " --codec " << c.coder->description();

        cmd << " --silent " << (silent_?"true":"false");

        return cmd.str();
    }
};

#include "subtitles.hpp"

class flimencoder
{
    const encoding_profile &profile_;
    input_reader* reader = nullptr;
    flimcompressor* compressor = nullptr;

    std::string out_pattern_ = "out-%06d.pgm"s;
    std::string change_pattern_ = "change-%06d.pgm"s;
    std::string diff_pattern_ = "diff-%06d.pgm"s;
    std::string target_pattern_ = "target-%06d.pgm"s;

    std::vector<subtitle> subtitles_;

    std::ofstream out_;
    std::back_insert_iterator<std::vector<uint8_t>>* out_toc_;

    double fps_ = 24;

    size_t poster_index_ = 0;
    double poster_ts_ = 0;
    image* poster_image_ = nullptr;
    image* poster_small_ = nullptr;
    image* poster_small_bw_ = nullptr;

    std::string comment_;

    std::string watermark_;

    size_t movie_size_ = 0;
    long fletcher_movie_size_ = 0;

    size_t cover_begin_;        /// Begin index of cover image
    size_t cover_end_;          /// End index of cover image

    size_t frame_from_image( size_t n ) const
    {
        return ticks_from_frame( n-1, fps_/profile_.fps_ratio() );
    }

    void make_posters(image& img) {
        poster_image_ = new image(img.W(), img.H());
        copy(*poster_image_, img, false);

        auto filters_string = profile_.filters();
        *poster_image_ = filter( *poster_image_, filters_string.c_str() );

        poster_small_ = new image( 128, 86 );
        copy( *poster_small_, *poster_image_, false );

        image previous( poster_small_->W(), poster_small_->H() );
        fill( previous, 0 );

        poster_small_bw_ = poster_small_;

        if (profile_.dither()==image::error_diffusion)
            error_diffusion( *poster_small_bw_, *poster_small_, previous, 0, *get_error_diffusion_by_name( profile_.error_algorithm() ), profile_.error_bleed(), profile_.error_bidi() );
        else if (profile_.dither()==image::ordered)
            ordered_dither( *poster_small_bw_, *poster_small_, previous );

        write_image( "/tmp/poster1.pgm", *poster_image_ );
        write_image( "/tmp/poster2.pgm", *poster_small_ );
        write_image( "/tmp/poster3.pgm", *poster_small_bw_ );
    }

    int clamp( double v, int a, int b )
    {
        int res = v+0.5;
        if (res<a) res = a;
        if (res>b) res = b;
        return res;
    }

    void write_frame(flimcompressor::frame& frm) {
        std::vector<uint8_t> movie; //  #### Should be 'frames'
        auto out_movie = std::back_inserter( movie );

        size_t frame_start = movie.size();

        write2( out_movie, frm.ticks );

        if (!profile_.silent())
        {
            write2( out_movie, frm.ticks*370+8 );           //  size of sound + header + size itself
            write2( out_movie, 0 );                       //  ffMode
            write4( out_movie, 65536 );                   //  rate
            write( out_movie, frm.audio );
        }
        else
        {
            write2( out_movie, 2 );           //  No sound
        }
        write2( out_movie, frm.video.size()+2 );
        write( out_movie, frm.video );

        //  TOC entry for current frame
        write2( *out_toc_, movie.size()-frame_start );

        movie_size_ += movie.size();

        out_.write(reinterpret_cast<char*>(movie.data()), movie.size());

        for (size_t i=0;i != movie.size();i+=2)
        {
            fletcher_movie_size_ += ((int)(movie[i]))*256+movie[i+1];
            fletcher_movie_size_ %= 65535;
        }
    }

    framegenerator<AVFrame*, AVFrame*> av_to_av_encoder() {
        using data_packet = std::tuple<AVFrame*, AVFrame*>;
        auto* f_reader = dynamic_cast<ffmpeg_reader*>(reader);

        AVPacket* pkt = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();

        int video_stream_index = f_reader->get_video_frame_index();
        int audio_stream_index = f_reader->get_audio_frame_index();

        if (!pkt || !frame) {
            throw std::runtime_error("Failed to allocate packet or frame");
        }

        while (av_read_frame(f_reader->get_format_context(), pkt) >= 0) {
            AVFrame* v_frame = nullptr;
            AVFrame* a_frame = nullptr;

            if (pkt->stream_index == video_stream_index) {
                f_reader->decode_video(frame, pkt, v_frame);
                // Encode it
            }
            else if (pkt->stream_index == audio_stream_index) {
                f_reader->decode_sound(frame, pkt, a_frame);
                // Encode it
            }

            av_packet_unref(pkt);

            // Compress decoded frames
            size_t local_ticks = compressor->get_local_ticks_until_next_frame();
            while(f_reader->can_extract_frames(local_ticks)) {
                image* img = f_reader->extract_video_frame();
                std::vector<sound_frame_t> sound_frames;

                if(!poster_image_ && f_reader->get_extracted_frames() >= poster_index_)
                    make_posters(*img);

                // Populate `sound_frames` vector
                for(size_t i = 0; i < local_ticks; i++) {
                    sound_frame_t* snd_ptr = f_reader->extract_sound_frame();
                    sound_frame_t snd = *snd_ptr;
                    sound_frames.push_back(snd);
                    delete snd_ptr;
                }

                compressor->compress(*img, sound_frames);
                delete img;
                sound_frames.clear();
            }

            // Write compressed frames
            while(poster_image_ && !compressor->frame_buffer_empty()) {
                flimcompressor::frame* encoded_frame = compressor->extract_frame();
                write_frame(*encoded_frame);
                delete encoded_frame;
            }

            if (v_frame || a_frame) {
                co_yield data_packet{v_frame, a_frame};
            }

        }

        av_packet_free(&pkt);
        av_frame_free(&frame);
    }

    void encode_av_to_av(const std::string &flim_pathname) {
        auto encoder = av_to_av_encoder();
        auto* f_reader = dynamic_cast<ffmpeg_reader*>(reader);

        time_t last_log_update = time(nullptr);

        size_t poster_index = poster_ts_*fps_/profile_.fps_ratio();

        if(poster_index < f_reader->get_frames_to_extract())
            poster_index_ = poster_index;

        // toc for video write
        std::vector<uint8_t> toc;
        out_toc_ = new std::back_insert_iterator<std::vector<uint8_t>>(std::back_inserter( toc ));

        std::stringstream tmp_file_path;
        tmp_file_path << flim_pathname << ".tmp" << std::endl;
        out_ = std::ofstream(tmp_file_path.str().c_str(), std::ios::binary);

        while(encoder.next()) {
            auto [v_frame, a_frame] = encoder.get_value();

            time_t current_time = time(nullptr);

            if(1 < (current_time - last_log_update)) {
                // Update logs every second
                last_log_update = current_time;
                log_progress();
            }

            if(v_frame)
                av_frame_free(&v_frame);
            if(a_frame)
                av_frame_free(&a_frame);
        }
        out_.close();


        std::vector<u_int8_t> global;
        auto out_global = std::back_inserter( global );

        write2( out_global, profile_.width() );                      //  width
        write2( out_global, profile_.height() );                      //  height
        write2( out_global, profile_.silent() );        //  1 = silent
        write4( out_global, compressor->get_compressed_frames() );            //  Framecount
        size_t total_ticks = compressor->get_ticks_qty();
        write4( out_global, total_ticks );              //  Tick count

        std::cout << "PROFILE BYTERATE " << profile_.byterate() << "\n";
        write2( out_global, profile_.byterate() );      //  Byterate

        framebuffer poster_fb{ *poster_small_bw_ };
        std::vector<u_int8_t> poster = poster_fb.raw_values_natural<u_int8_t>();

        std::vector<uint8_t> header;
        auto out_header = std::back_inserter( header );

        write2( out_header, 0x1 );                      //  Version
        write2( out_header, 4 );                        //  Entry count

        write2( out_header, 0x00 ); //  Info
        write4( out_header, 0 );  //  TOC offset
        write4( out_header, global.size() );            //  Frame count

        write2( out_header, 0x01 ); //  MOVIE
        write4( out_header, global.size() );
        write4( out_header, movie_size_ );

        write2( out_header, 0x02 ); //  TOC
        write4( out_header, global.size()+movie_size_ );
        write4( out_header, toc.size() );

        write2( out_header, 0x03 ); //  POSTER
        write4( out_header, global.size()+movie_size_+toc.size() );
        write4( out_header, poster.size() );

        //  Computes checksum
        long fletcher = 0;
//        if ((movie_size_%2)==1)
//            movie.push_back( 0x00 );
        for (size_t i=0;i!=header.size();i+=2)
        {
            fletcher += ((int)(header[i]))*256+header[i+1];
            fletcher %= 65535;
        }
        for (size_t i=0;i!=global.size();i+=2)
        {
            fletcher += ((int)(global[i]))*256+global[i+1];
            fletcher %= 65535;
        }
//        for (size_t i=0;i!=movie_size_;i+=2)
//        {
//            fletcher += ((int)(movie[i]))*256+movie[i+1];
//            fletcher %= 65535;
//        }
        fletcher += fletcher_movie_size_;
        fletcher %= 65535;
        for (size_t i=0;i!=toc.size();i+=2)
        {
            fletcher += ((int)(toc[i]))*256+toc[i+1];
            fletcher %= 65535;
        }
        for (size_t i=0;i!=poster.size();i+=2)
        {
            fletcher += ((int)(poster[i]))*256+poster[i+1];
            fletcher %= 65535;
        }

        std::ifstream src_tmp_file(tmp_file_path.str().c_str(), std::ios::binary);

        out_.open(flim_pathname.c_str(), std::ios::binary);

        char buffer[1024];
        std::fill( std::begin(buffer), std::end(buffer), 0 );
        strcpy( buffer, comment_.c_str() );
        out_.write(reinterpret_cast<char*>(&buffer), 1022);

        uint8_t b = fletcher/256;
        out_.write(reinterpret_cast<char*>(&b), 1);
        b = fletcher%256;
        out_.write(reinterpret_cast<char*>(&b), 1);

        out_.write(reinterpret_cast<char*>(header.data()), header.size());
        out_.write(reinterpret_cast<char*>(global.data()), global.size());

        out_ << src_tmp_file.rdbuf();
        src_tmp_file.close();

        out_.write(reinterpret_cast<char*>(toc.data()), toc.size());
        out_.write(reinterpret_cast<char*>(poster.data()), poster.size());

        out_.close();
    }

    void log_progress() {
        auto* f_reader = dynamic_cast<ffmpeg_reader*>(reader);
        std::clog << "Read " << f_reader->get_read_images() << " frames | Processed " << compressor->get_compressed_frames() << " frames\r" << std::flush;
    }

public:
//  #### remove in and audio
    flimencoder( const encoding_profile &profile ) : profile_{ profile } {}

    ~flimencoder() {
        delete reader;
        delete compressor;

        delete poster_image_;
        delete poster_small_;
        poster_small_bw_ = nullptr;

        delete out_toc_;
    }

    void set_fps( double fps ) { fps_ = fps; }
    void set_comment( const std::string comment ) { comment_ = comment; }
    void set_cover( size_t cover_begin, size_t cover_end ) { cover_begin_ = cover_begin; cover_end_ = cover_end; }
    void set_watermark( const std::string watermark ) { watermark_ = watermark; }
    void set_out_pattern( const std::string pattern ) { out_pattern_ = pattern; }
    void set_diff_pattern( const std::string pattern ) { diff_pattern_ = pattern; }
    void set_change_pattern( const std::string pattern ) { change_pattern_ = pattern; }
    void set_target_pattern( const std::string pattern ) { target_pattern_ = pattern; }
    void set_poster_ts( double poster_ts ) { poster_ts_ = poster_ts; }
    void set_subtitles( const std::vector<subtitle> &subtitles ) { subtitles_ = subtitles; /* yes, it is a copy */ }

    //  Encode all the frames
    void make_flim( const std::string flim_pathname, input_reader *r, const std::vector<std::unique_ptr<output_writer>> &writers )
    {  
        assert(r);

        reader = r;

        compressor = new flimcompressor(profile_.width(), profile_.height(), fps_ / profile_.fps_ratio(), subtitles_ );
        compressor->init_compressor( profile_.stability(),
                                     profile_.byterate(),
                                     profile_.group(),
                                     profile_.filters(),
                                     watermark_,
                                     profile_.codecs(),
                                     profile_.dither(),
                                     profile_.bars(),
                                     profile_.error_algorithm(),
                                     profile_.error_bleed(),
                                     profile_.error_bidi());

        encode_av_to_av(flim_pathname);
    }
};

#endif
