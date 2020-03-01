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

#ifndef RPI_QUALITY_CONFIG_
#define RPI_QUALITY_CONFIG_

#include <list>
#include <utility>

#include "absl/types/optional.h"
#include "common_types.h"
#include "config_media.h"
#include "rtc_base/numerics/moving_average.h"
#include "system_wrappers/include/clock.h"

// Currently, Only BWE-based QoS functions are implemented.
class QualityConfig {
   public:
    explicit QualityConfig();
    ~QualityConfig();

    struct Resolution {
        Resolution() : width_(0), height_(0), framerate_(0), bitrate_(0){};
        Resolution(int width, int height)
            : width_(width), height_(height), bitrate_(0){};
        int width_;
        int height_;
        int framerate_;
        int bitrate_;
    };

    struct ResolutionConfigEntry {
        ResolutionConfigEntry(int width, int height, int max_fps, int min_fps);
        int width_;
        int height_;
        int max_fps_;
        int min_fps_;
        int average_bandwidth_;
        int max_bandwidth_;
        int min_bandwidth_;
    };

    void ReportFrame(int frame_size);
    void ReportFrameRate(int framerate);
    void ReportTargetBitrate(int bitrate);  // kbps

    bool GetBestMatch(int target_bitrate,
                      QualityConfig::Resolution& resolution);
    bool GetBestMatch(QualityConfig::Resolution& resolution);
    bool GetInitialBestMatch(QualityConfig::Resolution& resolution);

   private:
    // media configuration sigleton reference
    ConfigMedia* config_media_;

    std::list<ResolutionConfigEntry> resolution_config_;
    int target_framerate_;
    int target_bitrate_;
    rtc::MovingAverage average_mf_; /* average motion factor */
    Resolution current_res_;
    bool use_dynamic_resolution_;
    bool use_initial_resolution_;
};

#endif  // RPI_QUALITY_CONFIG_
