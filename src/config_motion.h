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

#ifndef CONFIG_MOTION_H_
#define CONFIG_MOTION_H_

#include "config_defines.h"

//
// The config part for global config is required.
//
namespace config_motion {

CONFIG_DEFINE_H(MotionDetectionEnable, motion_detection_enable, bool);
CONFIG_DEFINE_H(MotionWidth, motion_width, int);
CONFIG_DEFINE_H(MotionHeight, motion_height, int);
CONFIG_DEFINE_H(MotionFps, motion_fps, int);
CONFIG_DEFINE_H(MotionBitrate, motion_bitrate, int);
CONFIG_DEFINE_H(MotionClearPercent, motion_clear_percent, int);
CONFIG_DEFINE_H(MotionClearWaitPeriod, motion_clear_wait_period, int);

CONFIG_DEFINE_H(MotionDirBase, motion_directory, std::string);
CONFIG_DEFINE_H(MotionFilePrefix, motion_file_prefix, std::string);
CONFIG_DEFINE_H(MotionFileSizeLimit, motion_file_size_limit, int);
CONFIG_DEFINE_H(MotionSaveImvFile, motion_save_imv_file, bool);

CONFIG_DEFINE_H(MotionEnableAnnotateText, motion_enable_annotate_text, bool);
CONFIG_DEFINE_H(MotionAnnotateText, motion_annotate_text, std::string);
CONFIG_DEFINE_H(MotionAnnotateTextSize, motion_annotate_text_size, int);

CONFIG_DEFINE_H(MotionBlobCancelThreshold, blob_cancel_threshold, float);
CONFIG_DEFINE_H(MotionBlobTrackingThreshold, blob_tracking_threshold, int);

CONFIG_DEFINE_H(MotionTotalFileSizeLimit, motion_file_total_size_limit, int);

bool config_load(const std::string config_filename);

}  // namespace config_motion

#endif  // CONFIG_MOTION_H_
