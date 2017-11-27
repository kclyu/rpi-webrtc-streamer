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

#include "config_defines.h"
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

CONFIG_DEFINE( FixedVideoResolution, fixed_video_resolution, 
        std::string, "640x480");

// audio related config
// this feature will require high CPU usage 
CONFIG_DEFINE( AudioProcessing, audio_processing_enable, bool, false );
CONFIG_DEFINE( AudioEchoCancel, audio_echo_cancel, bool, true );
CONFIG_DEFINE( AudioGainControl, audio_gain_control, bool, true );
CONFIG_DEFINE( AudioHighPassFilter, audio_highpass_filter, bool, true );
CONFIG_DEFINE( AudioNoiseSuppression, audio_noise_suppression, bool, true );
CONFIG_DEFINE( AudioLevelControl,  audio_level_control, bool, true );


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
// config loading and helper functions
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
    if ((framerate < 15) || (framerate > 30)) {
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
    DEFINE_CONFIG_LOAD_INT(MaxBitrate, max_bitrate );      
    // loading video rotation config
    DEFINE_CONFIG_LOAD_INT(VideoRotation, video_rotation ); 

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

    // loading flag for audio processing 
    DEFINE_CONFIG_LOAD_BOOL(AudioProcessing, audio_processing_enable);
    if( audio_processing_enable ) {
        // audio processing is enabled
        DEFINE_CONFIG_LOAD_BOOL(AudioEchoCancel,audio_echo_cancel);
        DEFINE_CONFIG_LOAD_BOOL(AudioGainControl,audio_gain_control);
        DEFINE_CONFIG_LOAD_BOOL(AudioHighPassFilter,audio_highpass_filter);
        DEFINE_CONFIG_LOAD_BOOL(AudioNoiseSuppression,audio_noise_suppression);
    };

    // level control is not depend on the audio processing
    DEFINE_CONFIG_LOAD_BOOL(AudioLevelControl,audio_level_control);

    return true;
}

}   // config_media namespace



