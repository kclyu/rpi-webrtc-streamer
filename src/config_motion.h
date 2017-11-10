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

#ifndef CONFIG_MOTION_H_
#define CONFIG_MOTION_H_

#include "webrtc/rtc_base/checks.h"


//
// The config part for global config is required.
//
namespace config_motion {
    extern bool motion_detection_enable;
    extern int motion_width;
    extern int motion_height;
    extern int motion_fps;
    extern int motion_bitrate;
    extern int motion_clear_percent;
    extern int motion_clear_wait_period;

    extern std::string motion_directory;
    extern std::string motion_file_prefix;

    extern int motion_file_size_limit; // K bytes
    extern bool motion_save_imv_file;

    extern bool motion_enable_annotate_text;
    extern std::string motion_annotate_text;
    extern int motion_annotate_text_size;

    extern float blob_cancel_threshold;
    extern int blob_tracking_threshold;

    extern int motion_file_total_size_limit; // Mbytes

    bool config_load(const std::string config_filename);

}   // media config name_space


#endif  // CONFIG_MOTION_H_
