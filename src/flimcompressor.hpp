#ifndef FILM_COMPRESSOR_INCLUDED__
#define FILM_COMPRESSOR_INCLUDED__

#include "image.hpp"
#include "framebuffer.hpp"
#include "imgcompress.hpp"
#include "compressor.hpp"

#include <utility>
#include <vector>
#include <array>
#include <bitset>
#include <algorithm>
#include <memory>

#include "reader.hpp"
#include "subtitles.hpp"

#define VERBOSE

using namespace std::string_literals;

/**
 * The flimcompressor manages higher aspects of the compression
 */
class flimcompressor
{
public:
    /// A frame contains encoded video delta and audio data for a single screen update
    struct frame
    {
        frame( size_t W, size_t H ) : source{ W, H }, result{ W, H } {}

        framebuffer source;         //  What we wanted to draw

        size_t ticks;               //  Number of ticks image is displayed
        std::vector<uint8_t> video; //  Encoded flim
        std::vector<uint8_t> audio;

        framebuffer result;         //  What we actually draw

        //  #### passing silent is inelegant: we should not generate audio data when silenced
        size_t get_size( bool silent ) { return video.size()+silent*audio.size(); }


        frame(
                const framebuffer &s,
                const size_t t,
                const std::vector<uint8_t> &v,
                const std::vector<uint8_t> &a,
                const framebuffer &r ) :
                source{s}, ticks{t}, video{v}, audio{a}, result{r} {}
    };

    struct codec_spec
    {
        uint8_t signature;
        double penality;
        std::shared_ptr<compressor> coder;
    };

    struct DitheringParameters
    {
        const bool bars_;                   //  Do we add bars when we resize the added image?  (note: maybe do some image normalizer class that does all conversion work)
        const std::string filters_;         //  Filters to apply
        const image::dithering dither_;     //  The kind of dither to apply
        const std::string error_algorithm_; //  Error algo
        const double stability_;            //  Stability of the transform
        const float error_bleed_;
        const bool error_bidi_;
        const std::string watermark_;       //  Unsure if this should be here or higher
    };

    /// This will dither a series of images, using the previous ones to minimize artifacts
    /// Size of the output is the same as the size of the initial image
    class Ditherer
    {
        size_t W_, H_;              //  Width and height of the generated image
        image dithered_image_;      //  The currently dithered image
        //  The initial image defines the size of all future images


        const DitheringParameters dp_;

    public:
        Ditherer( const image inital_image, const DitheringParameters dp ) :
                W_{ inital_image.W() },
                H_{ inital_image.H() },
                dithered_image_{ W_, H_ },
                dp_{dp}
        {
            //  Initial dithered image is black, we dither to whatever the initial image is
            dither( inital_image );
        }

        size_t W() const { return W_; }
        size_t H() const { return H_; }

        /// Dither the image according to the parameters
        /// Passed
        void dither( const image &img )
        {
            image resized_image( W_, H_ );   //  note: was 512x342
            copy( resized_image, img, dp_.bars_ );

            //  We filter the image of the "right size", for things like corners, etc...
            image filtered_image = filter( resized_image, dp_.filters_.c_str() );

            image dithered_image( W_, H_ ); //  The extract_video_frame dithered image

            if (dp_.dither_==image::error_diffusion)
                error_diffusion( dithered_image, filtered_image, dithered_image_, dp_.stability_, *get_error_diffusion_by_name( dp_.error_algorithm_ ), dp_.error_bleed_, dp_.error_bidi_ );
            else if (dp_.dither_==image::ordered)
                ordered_dither( dithered_image, filtered_image, dithered_image_ );
            else
                throw "Unknown dithering option";

            //  note: used to be done *after* (ie: in a local copy)
            round_corners( dithered_image );
            ::watermark( dithered_image, dp_.watermark_ );

            // char buffer[1024];
            // static int num = 0;
            // sprintf( buffer, "/tmp/foo-%04d.pgm", num );
            // num++;
            // write_image( buffer, dithered_image );

            //  The new dithered image is the previous one
            dithered_image_ = dithered_image;
        }

        //  The current dithered image
        const image current()
        {
            return dithered_image_;
        }
    };

    class SubtitleBurner
    {
        std::vector<subtitle> subtitles_;    //  The subtitles to burn
        // #### Should be a pair of const_iterators

    public:

        SubtitleBurner( const std::vector<subtitle> subtitles ) :
                subtitles_{ subtitles }
        {}

        //  Burn the subtitle for time into the image;
        void burn_into( image& img, double time )
        {
            if (subtitles_.size()>0)
            {
                //  Can do that way better with an iterator!
                if (time>=subtitles_.front().start)
                {
                    if (time<subtitles_.front().stop)
                    {
                        ::burn_subtitle( img, subtitles_.front().text.front() );   //  #### zero line subtitles will crash
                    }
                    else
                    {
                        subtitles_.erase( subtitles_.begin() ); //  We should flip the subtitles order in constructor!
                    }
                }
            }
        }
    };

    class EncodingResult
    {
        const codec_spec &codec_;           //  Used codec
        framebuffer image_;                 //  Resulting image
        const std::vector<uint8_t> data_;   //  Resulting data
        const double quality_;              //  Resulting quality

    public:
        EncodingResult(
                const codec_spec &codec,
                const framebuffer &current,
                const framebuffer &target,
                const size_t budget
        ) :
                codec_{ codec },
                image_{ current },
                data_{ codec_.coder->compress( image_, target,  budget*codec_.penality ) },
                quality_{ image_.proximity( target ) }
        {
            // char buffer[1024];
            // static int num = 0;
            // sprintf( buffer, "/tmp/foo-%04d.pgm", num );
            // num++;
            // write_image( buffer, image_.as_image() );
        }

        //  Encoded video with codec signature and trailer (#### why trailer?)
        std::vector<uint8_t> get_video_encoded_data() const
        {
            std::vector<uint8_t> result = { 0x00, 0x00, 0x00, codec_.signature };
            result.insert( std::end(result), std::begin(data_), std::end(data_) );
            return result;
        }

        double quality() const { return quality_; }
        const framebuffer &image() const { return image_; }
    };

    class CompressorHelper
    {
        Ditherer ditherer_;
        SubtitleBurner subtitle_burner_;
        framebuffer current_fb_;     //  The framebuffer displayed on screen at each step [#### check creation]
        const std::vector<codec_spec> codecs_;
        const double fps_;      //  Input fps
        const size_t byterate_;
        bool group_;

        size_t in_fr_;             //  Input frame
        size_t current_tick_;   //  Output tick number
        bool log_progress_ = true;
        // double total_q_ = 0;        //  Total quality
        static const size_t BucketCount = 1000;       //  Error distribution

        template <size_t N> class qhistogram
        {
            size_t total_ = 0;
            std::vector<size_t> samples_;
            bool verbose_ = true;

        public:
            qhistogram() : samples_(N+1) {}

            ~qhistogram()
            {
                dump();
            }

            void add( double quality )
            {
                assert( quality>=0 && quality<=1 );
                samples_[quality*N]++;
                total_++;
            }

            void dump() const
            {
                if (verbose_)
                {
                    std::clog << "+----------+--------+----------+----------+\n";
                    std::clog << "|     Q    | Frames |   Perc.  |  Cumul.  |\n";
                    std::clog << "|----------|--------|----------|----------|\n";
                }

                size_t cumulative = 0;
                double var99 = 0;
                double var98 = 0;
                double var95 = 0;
                for (size_t i=0;i!=N+1;i++)
                {
                    cumulative += samples_[i];
                    auto percent = (cumulative*1.0/total_);
                    if (percent>0.01 && var99==0)
                        var99 = i*1.0/N;
                    if (percent>0.02 && var98==0)
                        var98 = i*1.0/N;
                    if (percent>0.05 && var95==0)
                        var95 = i*1.0/N;
                    if (verbose_)
                        if (samples_[i])
                            fprintf( stderr, "| %7.3f%% | %6zu | %7.3f%% | %7.3f%% |\n", i*1.0/N*100, samples_[i], (samples_[i]*1.0/total_)*100, percent*100 );
                }
                if (verbose_)
                    std::clog << "+----------+--------+----------+----------+\n";
                std::clog << "99% of the frames are within "<< var99*100 << "% of the target pixels\n";
                std::clog << "98% of the frames are within "<< var98*100 << "% of the target pixels\n";
                std::clog << "95% of the frames are within "<< var95*100 << "% of the target pixels\n";
            }
        };

        qhistogram<BucketCount> histo_;

    public:
        CompressorHelper(
                Ditherer ditherer,
                SubtitleBurner subtitle_burner,
                const std::vector<codec_spec> codecs,
                const double fps,
                const size_t byterate,
                const bool group
        ) :
                ditherer_{std::move( ditherer )},
                subtitle_burner_{std::move( subtitle_burner )},
                current_fb_{ ditherer_.current() },
                codecs_{ codecs },
                fps_{ fps },
                byterate_{ byterate },
                group_{ group }
        {
            current_tick_ = 0;
            in_fr_ = 0;
            //current_audio_ = std::begin( audio_ );
        }

        size_t get_local_ticks_until_next_frame() const {
            size_t local_ticks = 1;

            if (group_){
                size_t ticks = ticks_from_frame( in_fr_ + 1, fps_ );
                ticks = ticks-current_tick_;
                local_ticks = ticks;
            }

            return local_ticks;
        }

        size_t get_num_compressed_frames() const {return in_fr_;}

        std::vector<frame>* process_image(const image &img_src, const std::vector<sound_frame_t> &snd_vector) {
            auto* frames = new std::vector<frame>();

            //  Dither the new image
            ditherer_.dither( img_src );
            image dest = ditherer_.current();
            subtitle_burner_.burn_into( dest, in_fr_/fps_ );

            //  True B&W packed image
            framebuffer fb{ dest };

            //  Let's see how many ticks we have to display this image
            in_fr_++;
            size_t next_tick = ticks_from_frame( in_fr_, fps_ );
            size_t ticks = next_tick-current_tick_;

            assert( ticks>0 );

            size_t local_ticks = 1;
            if (group_)
                local_ticks = ticks;

            for (size_t i=0;i!=ticks;i+=local_ticks) {
                //  Add as much audio as we have for the local ticks
                std::vector<uint8_t> audio;
                auto current_audio = std::begin( snd_vector );
                for (size_t i = 0; i != local_ticks; i++) {
                    sound_frame_t snd;
                    if (current_audio < std::end(snd_vector))
                        snd = *current_audio++;
                    std::copy(snd.begin(), snd.end(), std::back_inserter(audio));
                }

                std::stringstream filePath;
                filePath << "/tmp/test/" << in_fr_ << ".pgm";

                 write_image( filePath.str().c_str(), fb.as_image() );
                 //write_image( "/tmp/b.pgm", dest );

                //  Compute the video budget?
                size_t video_budget = byterate_*local_ticks;

                //  Encode within that budget with every codec
                std::vector<EncodingResult> encoding_results;
                std::transform(std::begin(codecs_), std::end(codecs_), std::back_inserter(encoding_results), [&]( auto &codec )-> EncodingResult
                { return EncodingResult(
                        codec,
                        current_fb_,
                        fb,
                        video_budget*codec.penality
                ); } );

                //  Find the result with the highest quality
                auto best_result = std::max_element(encoding_results.begin(), encoding_results.end(), [](const EncodingResult& r1, const EncodingResult& r2) { return r1.quality() < r2.quality(); } );

                //  Construct the frame with the best video and audio
                frame f{ fb, local_ticks, best_result->get_video_encoded_data(), audio, best_result->image() };

                frames->push_back( f );

                current_fb_ = best_result->image();
            }

            current_tick_ = next_tick;

            return frames;
        }

        std::vector<frame> get_frames() const { return frames_; }
    };

private:
    size_t W_;
    size_t H_;
    const double fps_;

    size_t extracted_frames_ = 0;

    CompressorHelper* helper = nullptr;

    std::vector<subtitle> subtitles_;
    std::deque<frame> frames_;

public:
    flimcompressor( size_t W, size_t H, double fps, const std::vector<subtitle> &subtitles ) : W_{W}, H_{H}, fps_{fps}, subtitles_{subtitles} {}

    ~flimcompressor() {
        delete helper;
    }

    // TODO : To remove
    const std::deque<frame> &get_frames() const { return frames_; }

    bool progress_ = true;

    static std::vector<std::string> split( const std::string s, const std::string delimiter )
    {
        size_t pos_start = 0, pos_end, delim_len = delimiter.length();
        std::string token;
        std::vector<std::string> res;

        while ((pos_end = s.find (delimiter, pos_start)) != std::string::npos)
        {
            token = s.substr (pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back (token);
        }

        res.push_back (s.substr (pos_start));
        return res;
    }

    static codec_spec make_codec( const std::string &spec_string, size_t W, size_t H )
    {
        auto spec_array = split( spec_string, ":" );
        auto name = spec_array[0];
        std::string parameters_string = "";

        if (spec_array.size()>1)
            parameters_string = spec_array[1];

        codec_spec spec;
        spec.signature = 0x00;
        spec.penality = 1;
        spec.coder = std::make_shared<null_compressor>( W, H );

        if (name=="z16")
        {
            spec.signature = 0x01;
            spec.penality = 0.45;
            spec.coder = std::make_shared<vertical_compressor<uint16_t>>( W, H, uint16_ruler::ruler );
        }
        else if (name=="z32")
        {   
            spec.signature = 0x02;
            spec.penality = 1.00;
            spec.coder = std::make_shared<vertical_compressor<uint32_t>>( W, H, uint32_ruler::ruler );
        }
        else if (name=="z32old")
        {   
            static bit_ruler<uint32_t> br32;
            spec.signature = 0x02;
            spec.penality = 1.00;
            spec.coder = std::make_shared<vertical_compressor<uint32_t>>( W, H, br32 );
        }
        else if (name=="invert")
        {
            spec.signature = 0x03;
            spec.penality = 1.00;
            spec.coder = std::make_shared<invert_compressor>( W, H );
        }
        else if (name=="lines")
        {   
            spec.signature = 0x04;
            spec.penality = 1.00;
            spec.coder = std::make_shared<copy_line_compressor>( W, H );
        }
        else if (name=="null")
        {   
            spec.signature = 0x00;
            spec.penality = 1.00;
            spec.coder = std::make_shared<null_compressor>( W, H );
        }
        else
        {
            std::clog << "Unknown codec : [" << name << "]\n";
            throw "Unknown codec";
        }

        for (auto &param_string:split(parameters_string,","))
        {
            auto v = split( param_string, "=" );
            std::string pname = v[0];;
            std::string pvalue = "";
            if (v.size()>1)
                pvalue = v[1];
            spec.coder->set_parameter( pname, pvalue );
        }

        return spec;
    }

    size_t get_local_ticks_until_next_frame() {
        if (helper)
            return helper->get_local_ticks_until_next_frame();
        else
            return 0;
    }

    size_t get_compressed_frames() {
        if(helper)
            return helper->get_num_compressed_frames();
        else
            return 0;
    }

    bool frame_buffer_empty() {
        return frames_.empty();
    }

    void compress(image& img, std::vector<sound_frame_t>& sound_frames) {
        if(!helper)
            return;

        std::vector<frame>* frames = helper->process_image(img, sound_frames);

        frames_.insert(frames_.end(), frames->begin(), frames->end());
        delete frames;
    }

    void init_compressor(double stability, size_t byterate, bool group, const std::string &filters, const std::string &watermark, const std::vector<codec_spec> &codecs, image::dithering dither, bool bars, const std::string error_algorithm, float error_bleed, bool error_bidi )
    {
        image previous( W_, H_ );
        fill( previous, 0 );

static bool generate_initial_frame = false;
// static bool loop_to_initial = true;

        if (generate_initial_frame)
        {
            //  #### We painfully extract what the first image should be
            image img0( W_,H_ );
//            copy( img0, images_[0], bars );
            image img1 = filter( img0, filters.c_str() );
            image img2( W_,H_ );
            if (dither==image::error_diffusion)
                error_diffusion( img2, img1, previous, stability, *get_error_diffusion_by_name( error_algorithm ), error_bleed, error_bidi );
            else if (dither==image::ordered)
                ordered_dither( img2, img1, previous );
            round_corners( img2 );
            ::watermark( img2, watermark );
            copy( previous, img2 );
            write_image( "/tmp/start.pgm", previous );
        }


#ifndef OLD_VERSION
    DitheringParameters dp { bars, filters, dither, error_algorithm, stability, error_bleed, error_bidi, watermark };
    Ditherer d{ previous, dp };
    SubtitleBurner sb{  subtitles_ };
    // TODO : CompressorHelper will become a property of flimcompressor. Might have to delete the old version of the code?
    helper = new CompressorHelper(d, sb, codecs, fps_, byterate, group );
    }

    frame* extract_frame() {
        if(frames_.empty()) {
            return nullptr;
        }

        auto* frm = new frame(frames_.front());
        frames_.pop_front();
        extracted_frames_++;

        return frm;
    }

};


#endif
