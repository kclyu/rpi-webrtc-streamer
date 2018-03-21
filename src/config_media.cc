/*
Copyright (c) 2017, rpi-webrtc-streamer Lyu,KeunChang

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <memory>
#include <string>
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/arraysize.h"

#include "config_media.h"
#include "utils.h"

namespace config_media {

////////////////////////////////////////////////////////////////////////////////
//
// media config key name and constants values
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Default Values
////////////////////////////////////////////////////////////////////////////////
static const char kConfigVideoResolutionDelimiter=',';
static const int  kDefaultVideoMaxFrameRate=30;

////////////////////////////////////////////////////////////////////////////////
// Config Key
////////////////////////////////////////////////////////////////////////////////

// video related config
CONFIG_DEFINE( MaxBitrate, max_bitrate, int, 3500000 );

CONFIG_DEFINE( Resolution4_3, resolution_4_3_enable, bool, true );
CONFIG_DEFINE( VideoRotation, video_rotation, int, 0);
CONFIG_DEFINE( VideoVFlip, video_vflip, bool, false );
CONFIG_DEFINE( VideoHFlip, video_hflip, bool, false );

CONFIG_DEFINE( VideoResolutionList43, video_resolution_list_4_3, std::string, \
    "320x240,400x300,512x384,640x480,1024x768,1152x864,1296x972,1640x1232" );
CONFIG_DEFINE( VideoResolutionList169, video_resolution_list_16_9, std::string, \
    "384x216,512x288,640x360,768x432,896x504,1024x576,1152x648,1280x720,1408x864,1920x1080");

CONFIG_DEFINE( VideoDynamicResolution, use_dynamic_video_resolution, bool, true );
CONFIG_DEFINE( VideoDynamicFps, use_dynamic_video_fps, bool, true);

CONFIG_DEFINE( FixedVideoResolution, fixed_video_resolution, std::string, "640x480");
CONFIG_DEFINE( FixedVideoFps, fixed_video_fps, int, 30);

// audio related config
// this feature will require high CPU usage 
CONFIG_DEFINE( AudioProcessing, audio_processing_enable, bool, false );
CONFIG_DEFINE( AudioEchoCancel, audio_echo_cancel, bool, true );
CONFIG_DEFINE( AudioGainControl, audio_gain_control, bool, true );
CONFIG_DEFINE( AudioHighPassFilter, audio_highpass_filter, bool, true );
CONFIG_DEFINE( AudioNoiseSuppression, audio_noise_suppression, bool, true );
CONFIG_DEFINE( AudioLevelControl,  audio_level_control, bool, true );

// video configuration config
CONFIG_DEFINE( VideoSharpness, video_sharpness, int, 0 );   // -100 - 100
CONFIG_DEFINE( VideoContrast, video_contrast, int, 0 );     // ~100 - 100
CONFIG_DEFINE( VideoBrightness, video_brightness, int, 50 );    // 0 - 100
CONFIG_DEFINE( VideoSaturation, video_saturation, int, 0 );   //-100 - 100
CONFIG_DEFINE( VideoEV, video_ev, int, 0 );
CONFIG_DEFINE( VideoExposureMode, video_exposure_mode, std::string, "auto" );
CONFIG_DEFINE( VideoFlickerMode, video_flicker_mode, std::string, "auto" );
CONFIG_DEFINE( VideoAwbMode, video_awb_mode, std::string, "auto" );
CONFIG_DEFINE( VideoDrcMode, video_drc_mode, std::string, "off" );
CONFIG_DEFINE( VideoStabilisation,  video_stabilisation, bool, false );

// Video Annotation 
CONFIG_DEFINE( VideoEnableAnnotateText, video_enable_annotate_text, bool, false );
CONFIG_DEFINE( VideoAnnotateText, video_annotate_text, std::string,"" );
CONFIG_DEFINE( VideoAnnotateTextSizeRatio, video_annotate_text_size_ratio, int, 3 );


////////////////////////////////////////////////////////////////////////////////
//
// Internal vaiables for storing configuration settings
//
////////////////////////////////////////////////////////////////////////////////

struct ResolutionConfig fixed_resolution(640,480);
int default_video_framerate = kDefaultVideoMaxFrameRate;

std::list <ResolutionConfig> resolution_list_4_3;
std::list <ResolutionConfig> resolution_list_16_9;


////////////////////////////////////////////////////////////////////////////////
//
// config validation helper functions
//
////////////////////////////////////////////////////////////////////////////////
bool validate__video_rotation(int video_rotation, int default_value ) {
    if( !((video_rotation == 0) || (video_rotation == 90) ||
                (video_rotation == 180) || (video_rotation == 270)) ) {
        RTC_LOG(LS_ERROR) << "Error in video roration value: " 
            << video_rotation << " is not a valid video rotation value";
        return false;
    }
    return true;
}

bool validate__video_maxbitrate(int video_maxbitrate, int default_value ) {
    // 17000000 values from RaspiVid.c bitrate for 1080p
    if ((video_maxbitrate < 200) || (video_maxbitrate > 17000000)) {
        RTC_LOG(LS_ERROR) << "Error in video max bitrate value: " 
            << video_rotation << " is not a valid video max bitrate value";
        return false;
    }
    return true;
}

bool validate__video_framerate(int framerate, int default_value ) {
    if ((framerate < 5) || (framerate > 30)) {
        RTC_LOG(LS_ERROR) << "Error in video frame rate value: " 
            << video_rotation << " is not a valid video frame rate value";
        return false;
    }
    return true;
}

bool parse_vidio_resolution(const std::string resolution_list, 
        std::list <ResolutionConfig> &resolution ) {
    std::stringstream ss(resolution_list);
    std::string token;
    int count=0;

    while( getline(ss, token, kConfigVideoResolutionDelimiter) ) {
        int width, height;
        if( utils::ParseVideoResolution(token, &width, &height ) == true )  {
            count++;
            resolution.push_back(ResolutionConfig(width,height));
        }
        else {
            RTC_LOG(LS_ERROR) << "Failed to add resolution : " << token;
        }
    }
    return (count?true:false);
}

// Returns true if the resolution exists in the resolution_list, 
// or false otherwise.
bool validate_resolution(int width, int height) {
    if( resolution_4_3_enable ) {
        for(std::list<ResolutionConfig>::iterator iter = 
                resolution_list_4_3.begin(); 
            iter != resolution_list_4_3.end(); iter++) {
            if( iter->width_ == width && iter->height_ == height)
                return true;
        }
    }
    else {
        for(std::list<ResolutionConfig>::iterator iter = 
                resolution_list_16_9.begin(); 
            iter != resolution_list_16_9.end(); iter++) {
            if( iter->width_ == width && iter->height_ == height)
                return true;
        }
    }
    return false;
}

// 
//  Video setting validation
// 
bool validate__value_sharpness( int sharpness, int /*default_value*/) {
    if( sharpness >= -100 &&  sharpness  <= 100 ) return true;
    RTC_LOG(LS_ERROR) << "Error in sharpness value: "  << sharpness;
    return false;
}

bool validate__value_contrast( int contrast, int /*default_value*/) {
    if( contrast >= -100 &&  contrast  <= 100 ) return true;
    RTC_LOG(LS_ERROR) << "Error in contrast value: "  << contrast;
    return false;
}

bool validate__value_brightness( int brightness, int /*default_value*/) {
    if( brightness >= 0 &&  brightness  <= 100 ) return true;
    RTC_LOG(LS_ERROR) << "Error in brightness value: "  << brightness;
    return false;
}

bool validate__value_saturation( int saturation, int /*default_value*/) {
    if( saturation >= -100 &&  saturation  <= 100 )
        return true;
    RTC_LOG(LS_ERROR) << "Error in saturation value: "  << saturation;
    return false;
}

bool validate__value_exposure_compensation( int ec, int /*default_value*/) {
    if( ec >= -10 &&  ec  <= 10 ) return true;
    RTC_LOG(LS_ERROR) << "Error in EC value: "  << ec;
    return false;
}

bool validate__value_exposure_mode(const std::string exposure_mode, 
        std::string /* defalut_value */ ) {
    if( check_optionvalue_exposure_mode(exposure_mode.c_str()))
        return true;
    RTC_LOG(LS_ERROR) << "Unknown exposure_mode: "  << exposure_mode;
    return false;
}

bool validate__value_flicker_mode(const std::string flicker_mode,
        std::string /* defalut_value */ ) {
    if( check_optionvalue_flicker_mode(flicker_mode.c_str()))
        return true;
    RTC_LOG(LS_ERROR) << "Unknown flicker mode: "  << flicker_mode;
    return false;
}

bool validate__value_awb_mode(const std::string awb_mode,
        std::string /* defalut_value */ ) {
    if( check_optionvalue_awb_mode(awb_mode.c_str()))
        return true;
    RTC_LOG(LS_ERROR) << "Unknown AWB mode: "  << awb_mode;
    return false;
}

bool validate__value_drc_mode(const std::string drc_mode,
        std::string /* defalut_value */ ) {
    if( check_optionvalue_drc_mode(drc_mode.c_str()))
        return true;
    RTC_LOG(LS_ERROR) << "Unknown DRC level : "  << drc_mode;
    return false;
}

// Returns true if the specifiec text size within valid range
// or return false otherwise.
bool validate_video_annotate_text_size_ratio(int text_size, int default_value) {
    if( text_size <  2 || text_size >= 10) {
        RTC_LOG(LS_ERROR) << "Annotate text size ratio is not valid\"" << text_size 
            << "\" text size ratio should be within 2 - 10, using default:"
            << default_value;
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// main config loading function
//
////////////////////////////////////////////////////////////////////////////////
bool config_load(const std::string config_filename) {
    rtc::OptionsFile config_(config_filename);

    if( config_.Load() == false ) {
        return false;
    };

    // loading max_bitrate
    DEFINE_CONFIG_LOAD_INT_VALIDATE(MaxBitrate, max_bitrate, 
            validate__video_maxbitrate);      
    // loading video rotation config
    DEFINE_CONFIG_LOAD_INT_VALIDATE(VideoRotation, video_rotation,
            validate__video_rotation ); 

    // loading vflip & hflip
    DEFINE_CONFIG_LOAD_BOOL(VideoVFlip, video_vflip);
    DEFINE_CONFIG_LOAD_BOOL(VideoHFlip, video_hflip);

    // loading 4:3 or 16:9 resolution config
    DEFINE_CONFIG_LOAD_BOOL(Resolution4_3, resolution_4_3_enable );

    // loading dynamic video resolution config
    //
    // The use_dynamic_video_resolution config value is used 
    // in QualityConfig :: GetBestMatch. When enabled, the resolution is changed 
    // to a resolution similar to the average bitrate value of the resolution.
    //
    // If it is disabled, it keeps the initial resolution set in InitEncoder. 
    // If you want to disable it, you must enable use_initial_video_resoltuion 
    // and set the desired resolution to fixed_video_resolution.
    DEFINE_CONFIG_LOAD_BOOL(VideoDynamicResolution, 
            use_dynamic_video_resolution );
    DEFINE_CONFIG_LOAD_BOOL(VideoDynamicFps, use_dynamic_video_fps );

    // loading default video resolution config
    DEFINE_CONFIG_LOAD_STR( VideoResolutionList43, 
            video_resolution_list_4_3);
    if( parse_vidio_resolution( video_resolution_list_4_3, 
                resolution_list_4_3 ) == false ) {
        // Loading default video resolution list
        parse_vidio_resolution( kDefaultVideoResolutionList43, 
                resolution_list_4_3 );
    };
    DEFINE_CONFIG_LOAD_STR( VideoResolutionList169, 
            video_resolution_list_16_9 );
    if( parse_vidio_resolution( video_resolution_list_16_9, 
                resolution_list_16_9 ) == false ) {
        // Loading default video resolution list
        parse_vidio_resolution( kDefaultVideoResolutionList169, 
                resolution_list_16_9 );
    };

    // loading flag for fixed video resolution 
    if( use_dynamic_video_resolution == false ) {
        // loading default video resolution config
        DEFINE_CONFIG_LOAD_STR( FixedVideoResolution, 
                fixed_video_resolution);
        if( config_loaded__FixedVideoResolution == true ){
            // need video width and height config 
            // to enable fixed_video_resolution
            DEFINE_CONFIG_LOAD_STR( FixedVideoResolution, 
                    fixed_video_resolution);

            int width, height;
            if( utils::ParseVideoResolution( fixed_video_resolution, 
                        &width, &height ) == true ) {
                if(validate_resolution(width, height) == true ) {
                    fixed_resolution.width_ = width;
                    fixed_resolution.height_ = height;
                }
                else {
                    RTC_LOG(LS_ERROR) << "Default resolution \"" 
                        <<  width << "x" << height << "\" is not valid";
                }
            };
        }
        else {
            RTC_LOG(LS_ERROR) 
                << "Fixed Video Resolution config is not found.";
            RTC_LOG(LS_ERROR) 
                << "Using default dyanmic video resolution instead of fixed video resolution.";
            use_dynamic_video_resolution = true;
        } 
    }
    if( use_dynamic_video_fps == false ) {
        DEFINE_CONFIG_LOAD_INT_VALIDATE( FixedVideoFps, fixed_video_fps,
            validate__video_framerate );
    }

    // loading flag for audio processing 
    DEFINE_CONFIG_LOAD_BOOL(AudioProcessing, audio_processing_enable);
    if( audio_processing_enable ) {
        // audio processing is enabled
        DEFINE_CONFIG_LOAD_BOOL(AudioEchoCancel,audio_echo_cancel);
        DEFINE_CONFIG_LOAD_BOOL(AudioGainControl,audio_gain_control);
        DEFINE_CONFIG_LOAD_BOOL(AudioHighPassFilter,audio_highpass_filter);
        DEFINE_CONFIG_LOAD_BOOL(AudioNoiseSuppression,audio_noise_suppression);
    };

    // Video Setting
    DEFINE_CONFIG_LOAD_INT_VALIDATE( VideoSharpness, video_sharpness,
            validate__value_sharpness );
    DEFINE_CONFIG_LOAD_INT_VALIDATE( VideoContrast, video_contrast,
            validate__value_contrast );
    DEFINE_CONFIG_LOAD_INT_VALIDATE( VideoBrightness, video_brightness,
            validate__value_brightness );
    DEFINE_CONFIG_LOAD_INT_VALIDATE( VideoSaturation, video_saturation,
            validate__value_saturation );
    DEFINE_CONFIG_LOAD_INT_VALIDATE( VideoEV, video_ev,
            validate__value_exposure_compensation );

    DEFINE_CONFIG_LOAD_STR_VALIDATE( VideoExposureMode, video_exposure_mode,
            validate__value_exposure_mode );
    DEFINE_CONFIG_LOAD_STR_VALIDATE( VideoFlickerMode, video_flicker_mode,
            validate__value_flicker_mode );
    DEFINE_CONFIG_LOAD_STR_VALIDATE( VideoAwbMode, video_awb_mode,
            validate__value_awb_mode );
    DEFINE_CONFIG_LOAD_STR_VALIDATE( VideoDrcMode, video_drc_mode,
            validate__value_drc_mode );
    DEFINE_CONFIG_LOAD_BOOL(VideoStabilisation,video_stabilisation);

    DEFINE_CONFIG_LOAD_BOOL(VideoEnableAnnotateText, video_enable_annotate_text );
    DEFINE_CONFIG_LOAD_STR(VideoAnnotateText, video_annotate_text);
    DEFINE_CONFIG_LOAD_INT_VALIDATE(VideoAnnotateTextSizeRatio, video_annotate_text_size_ratio,
        validate_video_annotate_text_size_ratio);

    return true;
}

}   // config_media namespace




