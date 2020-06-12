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

#include "raspi_quality_config.h"

#include "limits.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

static const int kAverageDuration = 30;  // Approximately 30 samples per second

static const float kKushGaugeConstant = 0.07;
static const int kMaxMontionFactor = 3;
static const int kMinMontionFactor = 1;  //  original value is 1 ~ 4
static const float kDefaultMontionFactor = 1.8f;
static const int kMonitorFactorWeight = 0.8f;

static const int kMaxFrameRate = 30;  // using 30 as raspberry pi max FPS

QualityConfig::ResolutionConfigEntry::ResolutionConfigEntry(int width,
                                                            int height,
                                                            int max_fps,
                                                            int min_fps)
    : width_(width), height_(height), max_fps_(max_fps), min_fps_(min_fps) {
    max_bandwidth_ = static_cast<int>(
        (width_ * height_ * max_fps * kKushGaugeConstant * kMaxMontionFactor) /
        1000);
    min_bandwidth_ = static_cast<int>(
        (width_ * height_ * min_fps * kKushGaugeConstant) / 1000);
    average_bandwidth_ =
        static_cast<int>((max_bandwidth_ + min_bandwidth_) / 2);
}

QualityConfig::QualityConfig()
    : target_framerate_(25),
      target_bitrate_(300) /*kbps*/,
      average_mf_(3 * 30) {
    config_media_ = ConfigMediaSingleton::Instance();
    std::list<ConfigMedia::VideoResolution> resolution_list =
        config_media_->GetVideoResolutionList();
    use_dynamic_resolution_ = config_media_->GetVideoDynamicResolution();

    for (std::list<ConfigMedia::VideoResolution>::iterator iter =
             resolution_list.begin();
         iter != resolution_list.end(); iter++) {
        resolution_config_.push_back(
            ResolutionConfigEntry(iter->width_, iter->height_, kMaxFrameRate,
                                  20 /* min_framerate */));
    }
}

QualityConfig::~QualityConfig() {}

void QualityConfig::ReportFrameRate(int framerate) {
    if (target_framerate_ == framerate) return;
    target_framerate_ = framerate;
}

void QualityConfig::ReportTargetBitrate(int bitrate /* kbps */) {
    target_bitrate_ = bitrate;
}

void QualityConfig::ReportFrame(int frame_length) {
    int motion_factor;
    // Moves the decimal point of the moving factor by 1000
    motion_factor = static_cast<int>(
        ((frame_length * 8) /
         (current_res_.width_ * current_res_.height_ * kKushGaugeConstant)) *
        1000);
    average_mf_.AddSample(motion_factor);
}

bool QualityConfig::GetBestMatch(QualityConfig::Resolution& resolution) {
    return GetBestMatch(target_bitrate_, resolution);
}

bool QualityConfig::GetInitialBestMatch(QualityConfig::Resolution& resolution) {
    Resolution candidate;
    if (use_dynamic_resolution_ == false) {
        // using fixed resolution
        config_media_->GetFixedVideoResolution(candidate.width_,
                                               candidate.height_);
        candidate.framerate_ = kMaxFrameRate;
        candidate.bitrate_ = static_cast<int>(
            (candidate.width_ * candidate.height_ * kMaxFrameRate *
             kKushGaugeConstant * kMaxMontionFactor) /
            1000);
        resolution = current_res_ = candidate;
        return true;
    }

    return GetBestMatch(target_bitrate_, resolution);
}

///////////////////////////////////////////////////////////////////////////////
// At present, it is based on the average value of Kush Gauge's 1 and 3 moving
// factors, but there is no evaluation as to whether it is appropriate.
// Simply find the nearest target_bitrate at the expected bitrate and change
// the resolution to the resolution.
////////////////////////////////////////////////////////////////////////////////
bool QualityConfig::GetBestMatch(int target_bitrate,
                                 QualityConfig::Resolution& resolution) {
    Resolution candidate;
    float motion_factor;
    int candidate_diff = std::numeric_limits<int>::max();
    int diff = 0;

    if (use_dynamic_resolution_ == false) {
        // The encoder does not use the Bitrate Estimation delivered by BWE,
        // but keeps the initially generated resolution.
        resolution = current_res_;
        return false;  // Do not change resoltuion
    };

    target_bitrate_ = target_bitrate;
    absl::optional<int> average_movingfactor =
        average_mf_.GetAverageRoundedDown();
    if (average_movingfactor) {
        motion_factor = *average_movingfactor / 1000;
        if (motion_factor < kDefaultMontionFactor)
            motion_factor = kDefaultMontionFactor;
    } else {
        motion_factor = kDefaultMontionFactor;
    }

    for (std::list<ResolutionConfigEntry>::iterator iter =
             resolution_config_.begin();
         iter != resolution_config_.end(); iter++) {
        int avg_bitrate =
            static_cast<int>((iter->width_ * iter->height_ * 25 *
                              kKushGaugeConstant * motion_factor) /
                             1000);
        diff = abs(avg_bitrate - target_bitrate);

        if (candidate_diff > diff) {
            candidate.width_ = iter->width_;
            candidate.height_ = iter->height_;
            candidate.bitrate_ = target_bitrate_;
            candidate.framerate_ = target_framerate_;
            // candidate.framerate_ = 25;
            candidate_diff = diff;
        }
    }

    if (candidate.width_ != current_res_.width_ ||
        candidate.height_ != current_res_.height_) {
        current_res_ = resolution = candidate;
        RTC_LOG(INFO) << "BestMatch Resolution " << candidate.width_ << "x"
                      << candidate.height_
                      << ", for target bitrate: " << target_bitrate_
                      << ", moving factor: " << motion_factor;

        return true;
    }
    resolution = candidate;
    return false;
}

