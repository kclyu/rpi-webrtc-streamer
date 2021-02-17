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

namespace {

// TODO validate the motion factor related constant values
const float kKushGaugeConstant = 0.07;
const int kMaxMontionFactor = 3;
const float kDefaultMontionFactor = 1.8f;

const int kMaxFrameRate = 30;
const int kMinFrameRate = 20;

}  // namespace

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
    use_dynamic_resolution_ = config_media_->GetVideoDynamicResolution();

    for (auto resolution : config_media_->GetVideoResolutionList()) {
        resolution_config_.push_back(ResolutionConfigEntry(
            resolution.width_, resolution.height_, kMaxFrameRate,
            kMinFrameRate /* min_framerate */));
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

void QualityConfig::ReportFrameSize(int frame_length) {
    int motion_factor;
    // Moves the decimal point of the moving factor by 1000
    motion_factor = static_cast<int>(
        ((frame_length * 8) /
         (current_res_.width_ * current_res_.height_ * kKushGaugeConstant)) *
        1000);
    average_mf_.AddSample(motion_factor);
}

wstreamer::VideoEncodingParams& QualityConfig::GetBestMatch() {
    return GetBestMatch(target_bitrate_);
}

wstreamer::VideoEncodingParams& QualityConfig::GetInitialBestMatch() {
    wstreamer::VideoEncodingParams candidate;
    if (use_dynamic_resolution_ == false) {
        // using fixed resolution
        config_media_->GetFixedVideoResolution(candidate.width_,
                                               candidate.height_);
        candidate.framerate_ = kMaxFrameRate;
        candidate.bitrate_ = static_cast<int>(
            (candidate.width_ * candidate.height_ * kMaxFrameRate *
             kKushGaugeConstant * kMaxMontionFactor) /
            1000);
        return current_res_ = candidate;
    }

    return GetBestMatch(target_bitrate_);
}

///////////////////////////////////////////////////////////////////////////////
// At present, it is based on the average value of Kush Gauge's 1 and 3 moving
// factors, but there is no evaluation as to whether it is appropriate.
// Simply find the nearest target_bitrate at the expected bitrate and change
// the resolution to the resolution.
////////////////////////////////////////////////////////////////////////////////
wstreamer::VideoEncodingParams& QualityConfig::GetBestMatch(
    int target_bitrate) {
    RTC_DCHECK(resolution_config_.size() > 0)
        << "length of resolution config is zero";
    wstreamer::VideoEncodingParams candidate;
    float motion_factor;
    int candidate_diff = std::numeric_limits<int>::max();
    int diff = 0;

    if (use_dynamic_resolution_ == false) {
        // The encoder does not use the Bitrate Estimation delivered by BWE,
        // but keeps the initially generated resolution.
        return current_res_;
    };

    target_bitrate_ = target_bitrate;
    absl::optional<int> average_movingfactor =
        average_mf_.GetAverageRoundedDown();
    if (average_movingfactor.has_value()) {
        motion_factor = *average_movingfactor / 1000;
        if (motion_factor < kDefaultMontionFactor)
            motion_factor = kDefaultMontionFactor;
    } else {
        motion_factor = kDefaultMontionFactor;
    }

    for (auto entry : resolution_config_) {
        // TODO: need to evaluate average bitrate which is calculated with
        // motion_factor
        int avg_bitrate =
            static_cast<int>((entry.width_ * entry.height_ * 25 *
                              kKushGaugeConstant * motion_factor) /
                             1000);
        diff = abs(avg_bitrate - target_bitrate);

        if (candidate_diff > diff) {
            candidate.width_ = entry.width_;
            candidate.height_ = entry.height_;
            candidate.bitrate_ = target_bitrate_;
            // TODO: need to check whether to keep the framerate when video
            // resolution changing
            candidate.framerate_ = target_framerate_;
            // candidate.framerate_ = 25;
            candidate_diff = diff;
        }
    }

    if (current_res_.IsSameResolution(candidate) == false) {
        current_res_ = candidate;
        RTC_LOG(INFO) << "BestMatch Resolution changed " << candidate.ToString()
                      << ", for target bitrate: " << target_bitrate_
                      << ", moving factor: " << motion_factor;
    }
    return current_res_;
}

