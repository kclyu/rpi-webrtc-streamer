/*
Copyright (c) 2018, rpi-webrtc-streamer Lyu,KeunChang

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

#include "config_defines.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/******************************************************************************
 *
 * camera setting value validation defined in raspicamcontrol.c
 *
******************************************************************************/

int check_optionvalue_exposure_mode( const char* str );
int check_optionvalue_flicker_mode(const char *str );
int check_optionvalue_awb_mode(const char *str );
int check_optionvalue_drc_mode(const char *str );

#ifdef __cplusplus
}	// extern "C"
#endif  // __cplusplus

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
    extern struct ResolutionConfig fixed_resolution;
    extern std::list <ResolutionConfig> resolution_list_4_3;
    extern std::list <ResolutionConfig> resolution_list_16_9;

    CONFIG_DEFINE_H( MaxBitrate, max_bitrate, int );
    CONFIG_DEFINE_H( Resolution4_3, resolution_4_3_enable, bool );
    CONFIG_DEFINE_H( VideoDynamicResolution, use_dynamic_video_resolution, bool);
    CONFIG_DEFINE_H( VideoDynamicFps, use_dynamic_video_fps, bool);
    CONFIG_DEFINE_H( VideoRotation, video_rotation, int);
    CONFIG_DEFINE_H( FixedVideoFps, fixed_video_fps, int);
    CONFIG_DEFINE_H( VideoVFlip, video_vflip, bool);
    CONFIG_DEFINE_H( VideoHFlip, video_hflip, bool);

    // this feature will require CPU usage because all of audio enhancement
    // feature is S/W based
    CONFIG_DEFINE_H( AudioProcessing, audio_processing_enable, bool);
    CONFIG_DEFINE_H( AudioEchoCancel, audio_echo_cancel, bool);
    CONFIG_DEFINE_H( AudioGainControl, audio_gain_control, bool);
    CONFIG_DEFINE_H( AudioHighPassFilter, audio_highpass_filter, bool);
    CONFIG_DEFINE_H( AudioNoiseSuppression, audio_noise_suppression, bool );
    CONFIG_DEFINE_H( AudioLevelControl,  audio_level_control, bool);

    CONFIG_DEFINE_H( VideoSharpness, video_sharpness, int );
    CONFIG_DEFINE_H( VideoContrast, video_contrast, int);
    CONFIG_DEFINE_H( VideoBrightness, video_brightness, int);
    CONFIG_DEFINE_H( VideoSaturation, video_saturation, int);
    CONFIG_DEFINE_H( VideoEV, video_ev, int);
    CONFIG_DEFINE_H( VideoExposureMode, video_exposure_mode, std::string);
    CONFIG_DEFINE_H( VideoFlickerMode, video_flicker_mode, std::string);
    CONFIG_DEFINE_H( VideoAwbMode, video_awb_mode, std::string);
    CONFIG_DEFINE_H( VideoDrcMode, video_drc_mode, std::string);
    CONFIG_DEFINE_H( VideoStabilisation, video_stabilisation, bool);

    // Annotation Text
    CONFIG_DEFINE_H( VideoEnableAnnotateText, video_enable_annotate_text, bool);
    CONFIG_DEFINE_H( VideoAonnotateText, video_annotate_text, std::string);
    CONFIG_DEFINE_H( VideoAnnotateTextSizeRatio, video_annotate_text_size_ratio, int);

    bool config_load(const std::string config_filename);

}   // media config name_space



#endif  // CONFIG_MEDIA_H_
