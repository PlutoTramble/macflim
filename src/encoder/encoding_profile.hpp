
#ifndef SRC_ENCODING_PROFILE_H
#define SRC_ENCODING_PROFILE_H


#include "flimcompressor.hpp"

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


#endif //SRC_ENCODING_PROFILE_H
