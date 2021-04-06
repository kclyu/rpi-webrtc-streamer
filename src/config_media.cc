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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config_media.h"

#include <math.h>

#include <memory>
#include <string>

#include "config_defs.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/strings/json.h"
#include "utils.h"

namespace {

const char kConfigVideoResolutionDelimiter = ',';

////////////////////////////////////////////////////////////////////////////////
//
// additional validation helper functions
//
////////////////////////////////////////////////////////////////////////////////

bool parse_vidio_resolution(const std::string resolution_list,
                            std::list<wstreamer::VideoResolution> &resolution) {
    std::list<wstreamer::VideoResolution> res;

    std::vector<std::string> splited_list;
    rtc::split(resolution_list, kConfigVideoResolutionDelimiter, &splited_list);
    for (auto token : splited_list) {
        int width, height;
        if (utils::ParseVideoResolution(token, &width, &height) == true) {
            res.push_back(wstreamer::VideoResolution(width, height));
        } else {
            RTC_LOG(LS_ERROR) << "Failed to parse resolution : " << token;
            // there is error, so do not assign local list to resolution
            return false;
        }
    }
    resolution = res;
    return true;
}

bool parse_video_roi(const std::string video_roi, wstreamer::VideoRoi &roi) {
    std::vector<std::string> splited_list;
    wstreamer::VideoRoi parsed_roi;
    rtc::split(video_roi, kConfigVideoResolutionDelimiter, &splited_list);
    if (splited_list.size() != 4) {
        RTC_LOG(LS_ERROR) << "Error, ROI value must be specified as 4 values :"
                          << video_roi;
        return false;
    }
    for (auto it = splited_list.begin(); it != splited_list.end(); ++it) {
        double value = 0.0f;
        int index = std::distance(splited_list.begin(), it);
        if (!(rtc::FromString(*it, &value) == true &&
              isgreater(value, 1.0f) == false &&
              parsed_roi.Set(static_cast<wstreamer::VideoRoi::ACCESS>(index),
                             value) == true)) {
            RTC_LOG(LS_ERROR) << "Error in Roi Value at index: " << index
                              << ", Value: " << *it;
            return false;
        }
    }

    if (parsed_roi.isValid() == false) return false;

    roi = parsed_roi;  // all validation passed, store
    return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigMediaSingleton interface
//
////////////////////////////////////////////////////////////////////////////////////////
ConfigMedia *ConfigMediaSingleton::Instance() {
    static ConfigMedia &config_media = *new ConfigMedia();
    return &config_media;
}

ConfigMediaSingleton::~ConfigMediaSingleton() {
    RTC_NOTREACHED() << "ConfigMediaSingleton should never be destructed.";
}

ConfigMediaSingleton::ConfigMediaSingleton() { RTC_NOTREACHED(); }

// Returns true if the resolution exists in the resolution_list,
// or false otherwise.
bool ConfigMedia::validate__resolution(int width, int height) {
    if (resolution_4_3_enable) {
        for (auto res : video_resolution_list_4_3_) {
            if (res.width_ == width && res.height_ == height) return true;
        }
    } else {
        for (auto res : video_resolution_list_16_9_) {
            if (res.width_ == width && res.height_ == height) return true;
        }
    }
    return false;
}

void ConfigMedia::GetMaxVideoResolution(int &width, int &height) const {
    int max_width = 0, max_height = 0;
    if (use_dynamic_video_resolution == true) {
        if (resolution_4_3_enable) {
            for (auto res : video_resolution_list_4_3_) {
                if ((res.width_ * res.height_) > (max_width * max_height)) {
                    max_width = res.width_;
                    max_height = res.height_;
                }
            }
        } else {
            for (auto res : video_resolution_list_16_9_) {
                if ((res.width_ * res.height_) > (max_width * max_height)) {
                    max_width = res.width_;
                    max_height = res.height_;
                }
            }
        }
    } else {
        max_width = fixed_resolution_.width_;
        max_height = fixed_resolution_.height_;
    }
    if (max_width * max_height == 0) {
        RTC_LOG(LS_ERROR) << "One of height/height is zero";
        max_width = 640;
        max_height = 480;  // using 640x480 as default resolution
    }
    RTC_LOG(INFO) << "Max Resolution : " << max_width << "x" << max_height;
    width = max_width;
    height = max_height;
}

wstreamer::VideoRoi &ConfigMedia::GetVideoROI(void) { return video_roi_; }

////////////////////////////////////////////////////////////////////////////////
//
// session RTC configuration
//
////////////////////////////////////////////////////////////////////////////////
bool ConfigMedia::SetSessionRtcConfig(const int sockid,
                                      const std::string rtc_config) {
    RTC_LOG(INFO) << "RTC configuration for session request: " << sockid
                  << ", RTC config: " << rtc_config;
    return true;
}
bool ConfigMedia::GetSessionRtcConfig(const int sockid,
                                      const std::string &rtc_config) {
    RTC_LOG(INFO) << "RTC configuration for session : " << sockid
                  << ", RTC config: " << rtc_config;
    return true;
}

bool ConfigMedia::RemoveSessionRtcConfig(const int sockid) {
    return (bool)session_rtcconfig_.erase(sockid);
}

////////////////////////////////////////////////////////////////////////////////
//
// config value validation functions
//
////////////////////////////////////////////////////////////////////////////////

DECLARE_METHOD_VALIDATOR(ConfigMedia, camera_select, int) {
    if (camera_select < 0 || camera_select > 2) {
        RTC_LOG(LS_ERROR) << "Error in video camera select num : "
                          << camera_select
                          << " is not a valid video camera number";
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_rotation, int) {
    if (!((video_rotation == 0) || (video_rotation == 90) ||
          (video_rotation == 180) || (video_rotation == 270))) {
        RTC_LOG(LS_ERROR) << "Error in video roration value: " << video_rotation
                          << " is not a valid video rotation value";
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, max_bitrate, int) {
    // 17000000 values from RaspiVid.c bitrate for 1080p
    if ((max_bitrate < 200) || (max_bitrate > 17000000)) {
        RTC_LOG(LS_ERROR) << "Error in video max bitrate value: " << max_bitrate
                          << " is not a valid video max bitrate value";
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, fixed_video_fps, int) {
    if ((fixed_video_fps < 5) || (fixed_video_fps > 30)) {
        RTC_LOG(LS_ERROR) << "Error in video frame rate value: "
                          << fixed_video_fps
                          << " is not a valid video frame rate value";
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, fixed_video_resolution, std::string) {
    int width, height;
    if (utils::ParseVideoResolution(fixed_video_resolution, &width, &height) ==
        true) {
        if (validate__resolution(width, height) == true) {
            fixed_resolution_.width_ = width;
            fixed_resolution_.height_ = height;
        } else {
            RTC_LOG(LS_ERROR) << "Fixed video resolution \"" << width << "x"
                              << height << "\" is not valid";
            return false;
        }
    };
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_resolution_list_4_3, std::string) {
    return parse_vidio_resolution(video_resolution_list_4_3,
                                  video_resolution_list_4_3_);
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_resolution_list_16_9, std::string) {
    return parse_vidio_resolution(video_resolution_list_16_9,
                                  video_resolution_list_16_9_);
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_roi, std::string) {
    return parse_video_roi(video_roi, video_roi_);
}

//
//  Video setting validation
//
DECLARE_METHOD_VALIDATOR(ConfigMedia, video_sharpness, int) {
    if (video_sharpness >= -100 && video_sharpness <= 100) return true;
    RTC_LOG(LS_ERROR) << "Error in sharpness value: " << video_sharpness;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_contrast, int) {
    if (video_contrast >= -100 && video_contrast <= 100) return true;
    RTC_LOG(LS_ERROR) << "Error in video_contrast value: " << video_contrast;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_brightness, int) {
    if (video_brightness >= 0 && video_brightness <= 100) return true;
    RTC_LOG(LS_ERROR) << "Error in brightness value: " << video_brightness;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_saturation, int) {
    if (video_saturation >= -100 && video_saturation <= 100) return true;
    RTC_LOG(LS_ERROR) << "Error in saturation value: " << video_saturation;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_ev, int) {
    if (video_ev >= -10 && video_ev <= 10) return true;
    RTC_LOG(LS_ERROR) << "Error in EV compensation value: " << video_ev;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_exposure_mode, std::string) {
    if (check_optionvalue_exposure_mode(video_exposure_mode.c_str()))
        return true;
    RTC_LOG(LS_ERROR) << "Unknown exposure_mode: " << video_exposure_mode;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_flicker_mode, std::string) {
    if (check_optionvalue_flicker_mode(video_flicker_mode.c_str())) return true;
    RTC_LOG(LS_ERROR) << "Unknown flicker mode: " << video_flicker_mode;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_awb_mode, std::string) {
    if (check_optionvalue_awb_mode(video_awb_mode.c_str())) return true;
    RTC_LOG(LS_ERROR) << "Unknown AWB mode: " << video_awb_mode;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_drc_mode, std::string) {
    if (check_optionvalue_drc_mode(video_drc_mode.c_str())) return true;
    RTC_LOG(LS_ERROR) << "Unknown DRC level : " << video_drc_mode;
    return false;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, video_annotate_text, std::string) {
    // MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2 is 256,
    // but, it is too big to display on video frame
    if (video_annotate_text.length() == 0 || video_annotate_text.length() > 64)
        return false;
    return true;
}

// Returns true if the specifiec text size within valid range
// or return false otherwise.
DECLARE_METHOD_VALIDATOR(ConfigMedia, video_annotate_text_size_ratio, int) {
    if (video_annotate_text_size_ratio < 2 ||
        video_annotate_text_size_ratio >= 10) {
        RTC_LOG(LS_ERROR)
            << "Annotate text size ratio is not valid\""
            << video_annotate_text_size_ratio
            << "\" text size ratio should be within 2 - 10, using default:"
            << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, audio_jitter_buffer_max_packets, int) {
    if (audio_jitter_buffer_max_packets < 0) {
        RTC_LOG(LS_ERROR) << "jitter buffer max packets is not valid\""
                          << audio_jitter_buffer_max_packets
                          << "\", using default: " << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, audio_jitter_buffer_min_delay_ms, int) {
    if (audio_jitter_buffer_min_delay_ms < 0) {
        RTC_LOG(LS_ERROR) << "jitter buffer main delay ms is not valid\""
                          << audio_jitter_buffer_min_delay_ms
                          << "\", using default: " << default_value;
        return false;
    }
    return true;
}

//
//  Still capture setting validation
//
DECLARE_METHOD_VALIDATOR(ConfigMedia, still_camera_num, int) {
    if (still_camera_num < 0 || still_camera_num > 3) {
        RTC_LOG(LS_ERROR) << "camera_num is not valid\"" << still_camera_num
                          << "\", using default: " << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, still_width, int) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, still_height, int) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, still_max_age, int) {
    if (still_max_age < 0 || still_max_age > 600) {  // 10 minutes
        RTC_LOG(LS_ERROR) << "Still image max_age is not valid\""
                          << still_max_age
                          << "\", using default: " << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, still_quality, int) {
    if (still_quality < 0 || still_quality > 100) {
        RTC_LOG(LS_ERROR) << "still_quality is not valid\"" << still_quality
                          << "\", using default: " << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, still_capture_interval, int) {
    if (still_capture_interval < 0 || still_capture_interval > 3600) {
        RTC_LOG(LS_ERROR) << "still_quality is not valid\""
                          << still_capture_interval
                          << "\", using default: " << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, still_directory, std::string) {
    if (!utils::IsFolder(still_directory)) {
        RTC_LOG(LS_ERROR) << "Path \"" << still_directory
                          << "\" is not directory. using default: "
                          << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, still_file_prefix, std::string) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMedia, still_file_extension, std::string) {
    // do nothing
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// main config loading function
//
////////////////////////////////////////////////////////////////////////////////
ConfigMedia::ConfigMedia(void)
    : media_optionfile_(nullptr), config_file_loaded_(false) {
    Reset();
}

ConfigMedia::~ConfigMedia(void) {}

std::list<wstreamer::VideoResolution> ConfigMedia::GetVideoResolutionList(
    void) {
    if (resolution_4_3_enable == true) {
        return video_resolution_list_4_3_;
    } else {
        return video_resolution_list_16_9_;
    }
}

bool ConfigMedia::GetFixedVideoResolution(int &width, int &height) {
    if (use_dynamic_video_resolution == true) {
        return false;
    }
    width = fixed_resolution_.width_;
    height = fixed_resolution_.height_;
    return true;
}

//
// Getter Method definition
//

#define _CR(name, config_var, config_remote_access, config_type, \
            default_value)                                       \
    config_type ConfigMedia::Get##name(void) const { return config_var; }
// ignore CR_L type
#define _CR_L(name, config_var, config_remote_access, config_type, \
              default_value)
#define _CR_B _CR
#define _CR_I _CR

#include "def/config_media.def"

//
// Setter Method definition
//
#define _CR(name, config_var, config_remote_access, config_type,            \
            default_value)                                                  \
    bool ConfigMedia::Set##name(config_type value) {                        \
        webrtc::MutexLock lock(&mutex_);                                    \
        if (validate_value__##config_var(value, default_value) == false)    \
            return false;                                                   \
        config_var = value;                                                 \
        if (isloaded__##config_var == false) isloaded__##config_var = true; \
        return true;                                                        \
    }
#define _CR_L _CR
#define _CR_B(name, config_var, config_remote_access, config_type,          \
              default_value)                                                \
    bool ConfigMedia::Set##name(config_type value) {                        \
        webrtc::MutexLock lock(&mutex_);                                    \
        config_var = value;                                                 \
        if (isloaded__##config_var == false) isloaded__##config_var = true; \
        return true;                                                        \
    }
#define _CR_I _CR

#include "def/config_media.def"

////////////////////////////////////////////////////////////////////////////////
//
// config loading and saving function
//
////////////////////////////////////////////////////////////////////////////////
void ConfigMedia::Reset(void) {
#define _CR(name, config_var, config_remote_access, config_type, \
            default_value)                                       \
    config_var = default_value;                                  \
    isloaded__##config_var = false;

#define _CR_L(name, config_var, config_remote_access, config_type, \
              default_value)                                       \
    config_var = default_value;                                    \
    isloaded__##config_var = false;                                \
    validate_value__##config_var(default_value, default_value);

#define _CR_B _CR
#define _CR_I _CR

#include "def/config_media.def"
}

bool ConfigMedia::Load(const std::string config_filename,
                       const bool dump_config) {
    media_optionfile_.reset(new rtc::OptionsFile(config_filename));
    if (media_optionfile_->Load() == false) {
        RTC_LOG(LS_ERROR) << "Failed to load config file, : " << config_file_
                          << ", so, using default.";
        config_file_loaded_ = false;
        return false;
    }
    config_file_loaded_ = true;
    config_file_ = config_filename;

#define _CR(name, config_var, config_remote_access, config_type,             \
            default_value)                                                   \
    {                                                                        \
        std::string config_value;                                            \
        if (media_optionfile_->GetStringValue(#config_var, &config_value) == \
            true) {                                                          \
            if (validate_value__##config_var(config_value, default_value) == \
                true) {                                                      \
                isloaded__##config_var = true;                               \
                config_var = config_value;                                   \
            } else {                                                         \
                RTC_LOG(LS_ERROR) << "Config " << #config_var                \
                                  << " error in value : " << config_value;   \
            };                                                               \
        }                                                                    \
    }

#define _CR_L _CR

#define _CR_B(name, config_var, config_remote_access, config_type,           \
              default_value)                                                 \
    {                                                                        \
        std::string config_value;                                            \
        if (media_optionfile_->GetStringValue(#config_var, &config_value) == \
            true) {                                                          \
            if (config_value.compare("true") == 0)                           \
                config_var = true;                                           \
            else if (config_value.compare("false") == 0)                     \
                config_var = false;                                          \
            else {                                                           \
                RTC_LOG(LS_ERROR) << "Config " << #config_var                \
                                  << " error in value : " << config_value;   \
            }                                                                \
        };                                                                   \
    }

#define _CR_I(name, config_var, config_remote_access, config_type,           \
              default_value)                                                 \
    {                                                                        \
        int config_value;                                                    \
        if (media_optionfile_->GetIntValue(#config_var, &config_value) ==    \
            true) {                                                          \
            if (validate_value__##config_var(config_value, default_value) == \
                true) {                                                      \
                isloaded__##config_var = true;                               \
                config_var = config_value;                                   \
            } else {                                                         \
                RTC_LOG(LS_ERROR) << "Config " << #config_var                \
                                  << " error in value : " << config_value;   \
            };                                                               \
        }                                                                    \
    }

#include "def/config_media.def"

    if (dump_config) DumpConfig();

    return true;
}

bool ConfigMedia::Save(void) {
#define _CR(name, config_var, config_remote_access, config_type,              \
            default_value)                                                    \
    {                                                                         \
        if (isloaded__##config_var == true) {                                 \
            if (media_optionfile_->SetStringValue(#config_var, config_var) == \
                true) {                                                       \
                RTC_LOG(INFO) << "Saving Config " << #config_var              \
                              << ", value : " << config_var;                  \
            };                                                                \
        };                                                                    \
    }

#define _CR_L _CR

#define _CR_B(name, config_var, config_remote_access, config_type,             \
              default_value)                                                   \
    {                                                                          \
        if (isloaded__##config_var == true) {                                  \
            if (config_var == true) {                                          \
                if (media_optionfile_->SetStringValue(#config_var, "true") ==  \
                    true) {                                                    \
                    RTC_LOG(INFO) << "Saving Config " << #config_var           \
                                  << ", value : true";                         \
                };                                                             \
            } else {                                                           \
                if (media_optionfile_->SetStringValue(#config_var, "false") == \
                    true) {                                                    \
                    RTC_LOG(INFO) << "Saving Config " << #config_var           \
                                  << ", value : false";                        \
                };                                                             \
            }                                                                  \
        };                                                                     \
    }

#define _CR_I(name, config_var, config_remote_access, config_type,         \
              default_value)                                               \
    {                                                                      \
        if (isloaded__##config_var == true) {                              \
            if (media_optionfile_->SetIntValue(#config_var, config_var) == \
                true) {                                                    \
                RTC_LOG(INFO) << "Saving Config " << #config_var           \
                              << ", value : " << config_var;               \
            };                                                             \
        };                                                                 \
    }

#include "def/config_media.def"

    if (media_optionfile_->Save() == false) {
        RTC_LOG(LS_ERROR) << "Failed to save config file, : " << config_file_;
        return false;
    }

    return true;
}

void ConfigMedia::DumpConfig(void) {
    RTC_LOG(INFO) << "Config Dump : "
                  << " Filename : " << config_file_;
    RTC_LOG(INFO) << "Config Rows : ";

#define _CR(name, config_var, config_remote_access, config_type,               \
            default_value)                                                     \
    {                                                                          \
        const char *loaded_str = (isloaded__##config_var) ? "*" : " ";         \
        RTC_LOG(INFO) << "Config: " << loaded_str << "\"" << #config_var       \
                      << "\": " << config_var << " -- (type: " << #config_type \
                      << ", default: " << default_value << ")";                \
    }

#define _CR_L(name, config_var, config_remote_access, config_type,             \
              default_value)                                                   \
    {                                                                          \
        char res_buffer[64];                                                   \
        std::string dump_list;                                                 \
        dump_list += "{";                                                      \
        for (std::list<wstreamer::VideoResolution>::iterator iter =            \
                 config_var##_.begin();                                        \
             iter != config_var##_.end(); iter++) {                            \
            snprintf(res_buffer, sizeof(res_buffer), "%dx%d,", iter->width_,   \
                     iter->height_);                                           \
            dump_list += res_buffer;                                           \
        }                                                                      \
        dump_list += "}";                                                      \
        const char *loaded_str = (isloaded__##config_var) ? "*" : " ";         \
        RTC_LOG(INFO) << "Config: " << loaded_str << "\"" << #config_var       \
                      << "\": " << config_var << " -- (type: " << #config_type \
                      << ", default: " << default_value << ")";                \
        RTC_LOG(INFO) << "Config: \"" << #config_var                           \
                      << "\" list: " << dump_list;                             \
    }

#define _CR_B(name, config_var, config_remote_access, config_type,           \
              default_value)                                                 \
    {                                                                        \
        const char *loaded_str = (isloaded__##config_var) ? "*" : " ";       \
        const char *bool_str = (config_var) ? "true" : "false";              \
        const char *bool_default_str = (default_value) ? "true" : "false";   \
        RTC_LOG(INFO) << "Config: " << loaded_str << "\"" << #config_var     \
                      << "\": " << bool_str << " -- (type: " << #config_type \
                      << ", default: " << bool_default_str << ")";           \
    }
#define _CR_I _CR

#include "def/config_media.def"
}

bool ConfigMedia::FromJson(const std::string &config_message,
                           std::string *config_updated, std::string &error) {
    Json::StyledWriter json_writer;
    Json::Reader json_reader;
    Json::Value json_value;
    Json::Value json_updated;

    if (json_reader.parse(config_message, json_value) == true) {
#define _CR(name, config_var, config_remote_access, config_type,              \
            default_value)                                                    \
    {                                                                         \
        config_type config_value;                                             \
        char err_msg[256];                                                    \
        Json::Value object;                                                   \
        std::string error_value;                                              \
        if (config_remote_access == true &&                                   \
            rtc::GetValueFromJsonObject(json_value, #config_var, &object) ==  \
                true) {                                                       \
            if (rtc::GetStringFromJson(object, &config_value) == true) {      \
                if (Set##name(config_value) == false) {                       \
                    rtc::GetStringFromJson(object, &error_value);             \
                    snprintf(err_msg, sizeof(err_msg),                        \
                             "failed to validate %s config value %s",         \
                             #config_var, error_value.c_str());               \
                    error = err_msg;                                          \
                    RTC_LOG(LS_ERROR) << "Failed to validate " << #config_var \
                                      << " config value " << error_value;     \
                    return false;                                             \
                }                                                             \
                json_updated[#config_var] = config_var;                       \
            } else {                                                          \
                /* is not int type */                                         \
                rtc::GetStringFromJson(object, &error_value);                 \
                snprintf(err_msg, sizeof(err_msg),                            \
                         "invalid %s config value %s", #config_var,           \
                         error_value.c_str());                                \
                error = err_msg;                                              \
                RTC_LOG(LS_ERROR)                                             \
                    << "invalid " << #config_var << " config value "          \
                    << error_value << " for " << #config_type                 \
                    << " type config";                                        \
                return false;                                                 \
            }                                                                 \
        }                                                                     \
    }

#define _CR_L _CR

#define _CR_B(name, config_var, config_remote_access, config_type,            \
              default_value)                                                  \
    {                                                                         \
        config_type config_value;                                             \
        char err_msg[256];                                                    \
        Json::Value object;                                                   \
        std::string error_value;                                              \
        if (config_remote_access == true &&                                   \
            rtc::GetValueFromJsonObject(json_value, #config_var, &object) ==  \
                true) {                                                       \
            if (rtc::GetBoolFromJson(object, &config_value) == true) {        \
                if (Set##name(config_value) == false) {                       \
                    rtc::GetStringFromJson(object, &error_value);             \
                    snprintf(err_msg, sizeof(err_msg),                        \
                             "failed to validate %s config value %s",         \
                             #config_var, error_value.c_str());               \
                    error = err_msg;                                          \
                    RTC_LOG(LS_ERROR) << "Failed to validate " << #config_var \
                                      << " config value " << error_value;     \
                    return false;                                             \
                }                                                             \
                json_updated[#config_var] = config_var;                       \
            } else {                                                          \
                /* is not bool type */                                        \
                rtc::GetStringFromJson(object, &error_value);                 \
                snprintf(err_msg, sizeof(err_msg),                            \
                         "invalid %s config value %s", #config_var,           \
                         error_value.c_str());                                \
                error = err_msg;                                              \
                RTC_LOG(LS_ERROR)                                             \
                    << "invalid " << #config_var << " config value "          \
                    << error_value << " for " << #config_type                 \
                    << " type config";                                        \
                return false;                                                 \
            }                                                                 \
        }                                                                     \
    }

#define _CR_I(name, config_var, config_remote_access, config_type,            \
              default_value)                                                  \
    {                                                                         \
        config_type config_value;                                             \
        char err_msg[256];                                                    \
        Json::Value object;                                                   \
        std::string error_value;                                              \
        if (config_remote_access == true &&                                   \
            rtc::GetValueFromJsonObject(json_value, #config_var, &object) ==  \
                true) {                                                       \
            if (rtc::GetIntFromJson(object, &config_value) == true) {         \
                if (Set##name(config_value) == false) {                       \
                    rtc::GetStringFromJson(object, &error_value);             \
                    snprintf(err_msg, sizeof(err_msg),                        \
                             "failed to validate %s config value %s",         \
                             #config_var, error_value.c_str());               \
                    error = err_msg;                                          \
                    RTC_LOG(LS_ERROR) << "Failed to validate " << #config_var \
                                      << " config value " << error_value;     \
                    return false;                                             \
                }                                                             \
                json_updated[#config_var] = config_var;                       \
            } else {                                                          \
                /* is not int type */                                         \
                rtc::GetStringFromJson(object, &error_value);                 \
                snprintf(err_msg, sizeof(err_msg),                            \
                         "invalid %s config value %s", #config_var,           \
                         error_value.c_str());                                \
                error = err_msg;                                              \
                RTC_LOG(LS_ERROR)                                             \
                    << "invalid " << #config_var << " config value "          \
                    << error_value << " for " << #config_type                 \
                    << " type config";                                        \
                return false;                                                 \
            }                                                                 \
        }                                                                     \
    }

#include "def/config_media.def"

    } else {
        std::string parse_error_msg = "Parsing Error : ";
        // Failed to parse json
        RTC_LOG(LS_ERROR) << "Failed to parse JSON config string: "
                          << config_message;
        error = parse_error_msg + json_reader.getFormatedErrorMessages();
        return false;
    }
    if (config_updated != nullptr) {
        *config_updated = json_writer.write(json_updated);
    };
    return true;
}

bool ConfigMedia::ToJson(std::string &config_message) {
    Json::StyledWriter json_writer;
    Json::Value json_config;

    // generate json key when the remote config access is true
#define _CR(name, config_var, config_remote_access, config_type, \
            default_value)                                       \
    if (config_remote_access == true) json_config[#config_var] = config_var;

#define _CR_L _CR
#define _CR_B _CR
#define _CR_I _CR

#include "def/config_media.def"

    config_message = json_writer.write(json_config);
    return true;
}
