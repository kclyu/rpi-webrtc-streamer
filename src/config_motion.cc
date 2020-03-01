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

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "utils.h"

namespace config_motion {

///////////////////////////////////////////////////////////////////////////////////////////
//
// motion config key name and constants values
//
///////////////////////////////////////////////////////////////////////////////////////////

CONFIG_DEFINE(MotionDetectionEnable, motion_detection_enable, bool, false);
CONFIG_DEFINE(MotionWidth, motion_width, int, 1024);
CONFIG_DEFINE(MotionHeight, motion_height, int, 768);
CONFIG_DEFINE(MotionFps, motion_fps, int, 30);
CONFIG_DEFINE(MotionBitrate, motion_bitrate, int, 3500);
CONFIG_DEFINE(MotionClearPercent, motion_clear_percent, int, 5);
CONFIG_DEFINE(MotionClearWaitPeriod, motion_clear_wait_period, int, 5000);

CONFIG_DEFINE(MotionDirBase, motion_directory, std::string,
              "/opt/rws/motion_captured");
CONFIG_DEFINE(MotionFilePrefix, motion_file_prefix, std::string, "motion");
CONFIG_DEFINE(MotionFileSizeLimit, motion_file_size_limit, int,
              6000);  // kbytes
CONFIG_DEFINE(MotionSaveImvFile, motion_save_imv_file, bool, false);

CONFIG_DEFINE(MotionEnableAnnotateText, motion_enable_annotate_text, bool,
              true);
CONFIG_DEFINE(MotionAnnotateText, motion_annotate_text, std::string, "");
CONFIG_DEFINE(MotionAnnotateTextSize, motion_annotate_text_size, int, 32);

CONFIG_DEFINE(MotionBlobCancelThreshold, blob_cancel_threshold, float, 0.5);
CONFIG_DEFINE(MotionBlobTrackingThreshold, blob_tracking_threshold, int, 15);

CONFIG_DEFINE(MotionTotalFileSizeLimit, motion_file_total_size_limit, int,
              4000);  // Mbytes

///////////////////////////////////////////////////////////////////////////////////////////
//
// config validation helper functions
//
///////////////////////////////////////////////////////////////////////////////////////////

// Returns true if the specifiec directory path exists
// or return false otherwise.
bool validate_motion_directory_path(std::string path,
                                    std::string default_value) {
    if (!utils::IsFolder(path)) {
        RTC_LOG(LS_ERROR) << "Path \"" << path
                          << "\" is not directory. using default:"
                          << default_value;
        return false;
    }
    return true;
}

// Returns true if the specifiec text size within valid range
// or return false otherwise.
bool validate_motion_annotate_text_size(int text_size, int default_value) {
    if (text_size < 6 || text_size >= 160) {
        RTC_LOG(LS_ERROR)
            << "Annotate text size is not valid\"" << text_size
            << "\" text size should be within 6 - 160, using default:"
            << default_value;
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
//
// motion config loading functions
//
///////////////////////////////////////////////////////////////////////////////////////////

bool config_load(const std::string config_filename) {
    rtc::OptionsFile config_(config_filename);

    if (config_.Load() == false) {
        return false;
    };

    // loading MotionDetectionEnable
    DEFINE_CONFIG_LOAD_BOOL(MotionDetectionEnable, motion_detection_enable);

    // do not load motion config when the motion_detection_enable is false
    if (motion_detection_enable == false) return true;

    // loading Motion Config
    DEFINE_CONFIG_LOAD_INT(MotionWidth, motion_width);
    DEFINE_CONFIG_LOAD_INT(MotionHeight, motion_height);
    DEFINE_CONFIG_LOAD_INT(MotionFps, motion_fps);
    DEFINE_CONFIG_LOAD_INT(MotionBitrate, motion_bitrate);
    DEFINE_CONFIG_LOAD_INT(MotionClearPercent, motion_clear_percent);
    DEFINE_CONFIG_LOAD_INT(MotionClearWaitPeriod, motion_clear_wait_period);

    DEFINE_CONFIG_LOAD_STR_VALIDATE(MotionDirBase, motion_directory,
                                    validate_motion_directory_path);
    DEFINE_CONFIG_LOAD_STR(MotionFilePrefix, motion_file_prefix);

    DEFINE_CONFIG_LOAD_INT(MotionFileSizeLimit,
                           motion_file_size_limit);  // kbytes
    DEFINE_CONFIG_LOAD_BOOL(MotionSaveImvFile, motion_save_imv_file);

    DEFINE_CONFIG_LOAD_BOOL(MotionEnableAnnotateText,
                            motion_enable_annotate_text);
    DEFINE_CONFIG_LOAD_STR(MotionAnnotateText, motion_annotate_text);
    DEFINE_CONFIG_LOAD_INT_VALIDATE(MotionAnnotateTextSize,
                                    motion_annotate_text_size,
                                    validate_motion_annotate_text_size);

    DEFINE_CONFIG_LOAD_FLOAT(MotionBlobCancelThreshold, blob_cancel_threshold);
    DEFINE_CONFIG_LOAD_INT(MotionBlobTrackingThreshold,
                           blob_tracking_threshold);

    DEFINE_CONFIG_LOAD_INT(MotionTotalFileSizeLimit,
                           motion_file_total_size_limit);  // Mbytes
    return true;
}

}  // namespace config_motion
