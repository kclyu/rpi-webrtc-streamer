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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config_motion.h"

#include <memory>
#include <string>

#include "config_defs.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "utils.h"

namespace {

constexpr int kMaxClearWaitPeriod = 10000;  // 10 seconds
constexpr int kMinClearWaitPeriod = 2000;   // 10 seconds
constexpr int kMaxClearPercent = 10;
constexpr int kMinClearPercent = 3;
constexpr int kMaxMotionFps = 30;
constexpr int kMaxAnnotationTextLength = 256;

}  // namespace

///////////////////////////////////////////////////////////////////////////////////////////
//
// config validation helper functions
//
///////////////////////////////////////////////////////////////////////////////////////////
DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_width, int) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_height, int) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_bitrate, int) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_clear_wait_period, int) {
    if (motion_clear_wait_period < kMinClearWaitPeriod ||
        motion_clear_wait_period > kMaxClearWaitPeriod) {
        RTC_LOG(LS_ERROR) << "Motion Clear Wait period \""
                          << motion_clear_wait_period
                          << "\" is not vali. using default: " << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_clear_percent, int) {
    if (motion_clear_percent < kMinClearPercent ||
        motion_clear_percent > kMaxClearPercent) {
        RTC_LOG(LS_ERROR) << "Motion Clear Percent\"" << motion_clear_percent
                          << "\" is not vali. using default: " << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_fps, int) {
    if (motion_fps < 0 || motion_fps > kMaxMotionFps) {
        RTC_LOG(LS_ERROR) << "Motion FPS  \"" << motion_fps
                          << "\" is not vali. using default: " << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_file_prefix, std::string) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_file_size_limit, int) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_file_total_size_limit, int) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, blob_tracking_threshold, int) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, blob_cancel_threshold, float) {
    // do nothing
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_directory, std::string) {
    if (!utils::IsFolder(motion_directory)) {
        RTC_LOG(LS_ERROR) << "Path \"" << motion_directory
                          << "\" is not directory. using default: "
                          << default_value;
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_annotate_text, std::string) {
    if (motion_annotate_text.length() > kMaxAnnotationTextLength) {
        RTC_LOG(LS_ERROR) << "Annotate text string length  is not valid\""
                          << motion_annotate_text.length()
                          << "\" text size string length should be within 6 - "
                             "256, using default: \"\"";
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigMotion, motion_annotate_text_size, int) {
    if (motion_annotate_text_size < 6 || motion_annotate_text_size >= 160) {
        RTC_LOG(LS_ERROR)
            << "Annotate text size is not valid\"" << motion_annotate_text_size
            << "\" text size should be within 6 - 160, using default:"
            << default_value;
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// ConfigStreamer, Getter methods
//
////////////////////////////////////////////////////////////////////////////////

#define _CR(name, config_var, config_remote_access, config_type, \
            default_value)                                       \
    config_type ConfigMotion::Get##name(void) const { return config_var; }

#define _CR_B _CR
#define _CR_I _CR
#define _CR_F _CR

#include "def/config_motion.def"

////////////////////////////////////////////////////////////////////////////////
//
// ConfigMotion
//
////////////////////////////////////////////////////////////////////////////////
ConfigMotion::ConfigMotion(const std::string& config_file, bool dump_config)
    : config_loaded_(false),
      config_file_(config_file),
      dump_config_(dump_config) {
    // trying to check flag config_file is regular file
    if (utils::IsFile(config_file_) == false) {
        RTC_LOG(LS_ERROR) << "Failed to find motion configuration file :"
                          << config_file_;
    }
}

bool ConfigMotion::Load() {
    options_file_.reset(new rtc::OptionsFile(config_file_));
    if (options_file_->Load() == false) {
        RTC_LOG(LS_ERROR) << "Failed to load config options:" << config_file_;
        return false;
    }

    // Inititialize the config variables with default value
#define _CR(name, config_var, config_remote_access, config_type, \
            default_value)                                       \
    config_var = default_value;                                  \
    isloaded__##config_var = false;

#define _CR_B _CR
#define _CR_I _CR
#define _CR_F _CR

#include "def/config_motion.def"

#define _CR(name, config_var, config_remote_access, config_type,             \
            default_value)                                                   \
    {                                                                        \
        std::string config_value;                                            \
        if (options_file_->GetStringValue(#config_var, &config_value) ==     \
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

#define _CR_B(name, config_var, config_remote_access, config_type,         \
              default_value)                                               \
    {                                                                      \
        std::string config_value;                                          \
        if (options_file_->GetStringValue(#config_var, &config_value) ==   \
            true) {                                                        \
            if (config_value.compare("true") == 0)                         \
                config_var = true;                                         \
            else if (config_value.compare("false") == 0)                   \
                config_var = false;                                        \
            else {                                                         \
                RTC_LOG(LS_ERROR) << "Config " << #config_var              \
                                  << " error in value : " << config_value; \
            }                                                              \
        };                                                                 \
    }

#define _CR_I(name, config_var, config_remote_access, config_type,            \
              default_value)                                                  \
    {                                                                         \
        int config_value;                                                     \
        if (options_file_->GetIntValue(#config_var, &config_value) == true) { \
            if (validate_value__##config_var(config_value, default_value) ==  \
                true) {                                                       \
                isloaded__##config_var = true;                                \
                config_var = config_value;                                    \
            } else {                                                          \
                RTC_LOG(LS_ERROR) << "Config " << #config_var                 \
                                  << " error in value : " << config_value;    \
            };                                                                \
        }                                                                     \
    }

#define _CR_F(name, config_var, config_remote_access, config_type,         \
              default_value)                                               \
    {                                                                      \
        std::string config_value;                                          \
        float config_value_float;                                          \
        if (options_file_->GetStringValue(#config_var, &config_value) ==   \
                true &&                                                    \
            rtc::FromString(config_value, &config_value_float) == true) {  \
            if (validate_value__##config_var(config_value_float,           \
                                             default_value) == true) {     \
                isloaded__##config_var = true;                             \
                config_var = config_value_float;                           \
            } else {                                                       \
                RTC_LOG(LS_ERROR) << "Config " << #config_var              \
                                  << " error in value : " << config_value; \
            };                                                             \
        }                                                                  \
    }

#include "def/config_motion.def"

    config_loaded_ = true;
    if (dump_config_) DumpConfig();
    return true;
}

void ConfigMotion::DumpConfig(void) {
    RTC_LOG(INFO) << "Config Dump : "
                  << " Filename : " << config_file_;
    RTC_LOG(INFO) << "Config Rows : ";

#define _CR(name, config_var, config_remote_access, config_type,               \
            default_value)                                                     \
    {                                                                          \
        const char* loaded_str = (isloaded__##config_var) ? "*" : " ";         \
        RTC_LOG(INFO) << "Config: " << loaded_str << "\"" << #config_var       \
                      << "\": " << config_var << " -- (type: " << #config_type \
                      << ", default: " << default_value << ")";                \
    }

#define _CR_B(name, config_var, config_remote_access, config_type,           \
              default_value)                                                 \
    {                                                                        \
        const char* loaded_str = (isloaded__##config_var) ? "*" : " ";       \
        const char* bool_str = (config_var) ? "true" : "false";              \
        const char* bool_default_str = (default_value) ? "true" : "false";   \
        RTC_LOG(INFO) << "Config: " << loaded_str << "\"" << #config_var     \
                      << "\": " << bool_str << " -- (type: " << #config_type \
                      << ", default: " << bool_default_str << ")";           \
    }
#define _CR_I _CR
#define _CR_F _CR

#include "def/config_motion.def"
}

ConfigMotion::~ConfigMotion() {}

