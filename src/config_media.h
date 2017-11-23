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

#ifndef CONFIG_MEDIA_H_
#define CONFIG_MEDIA_H_

#include "rtc_base/checks.h"

#include "rtc_base/fileutils.h"
#include "rtc_base/optionsfile.h"
#include "rtc_base/pathutils.h"

//
// The config part for global media config is required.
//
namespace config_media {

    struct ResolutionConfig {
        explicit ResolutionConfig(int width, int height ) :
            width_(width),height_(height) {};
        virtual ~ResolutionConfig() {};
        int width_;
        int height_;
    };

    extern int max_bitrate;
    extern bool resolution_4_3_enable;

    extern struct ResolutionConfig initial_resolution;
    extern int initial_video_framerate;
    extern bool use_initial_video_resolution;
    extern bool use_dynamic_video_resolution;
    extern std::list <ResolutionConfig> resolution_list_4_3;
    extern std::list <ResolutionConfig> resolution_list_16_9;

    // this feature will require CPU usage because all of audio enhancement 
    // feature is S/W based
    extern bool audio_processing_enable;
    extern bool audio_echo_cancel;
    extern bool audio_gain_control;
    extern bool audio_highpass_filter;
    extern bool audio_noise_suppression;
    extern bool audio_level_control;

    extern int video_rotation;
    extern bool video_hflip;
    extern bool video_vflip;

    bool config_load(const std::string config_filename);

}   // media config name_space



#endif  // CONFIG_MEDIA_H_
