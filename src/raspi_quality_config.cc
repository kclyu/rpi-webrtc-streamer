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


#include "limits.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#include "config_media.h"
#include "raspi_quality_config.h"


static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 35;
static const int kPacketLossThreshold = 8;   // Approximately 3.1%
static const int kRttMaxThreshold = 200;     // 200 ms maximum
static const int kAverageDuration =  3 * 30; // Approximately 30 samples per second

//
static const float kKushGaugeConstant = 0.07;
static const int kMaxMontionFactor = 3;
static const int kMinMontionFactor = 1;   //  original value is 1 ~ 4
                                          // but quality config will use up to 3

static const int kMaxFrameRate = 30; // using 30 as raspberry pi max FPS


QualityConfig::ResolutionConfigEntry::ResolutionConfigEntry (int width,
        int height, int min_fps, int max_fps)
    : width_(width), height_(height), max_fps_(max_fps), min_fps_(min_fps) {

    max_bandwidth_  = static_cast<int>((width_ * height_ * max_fps *
                kKushGaugeConstant * kMaxMontionFactor )/1000);
    min_bandwidth_  = static_cast<int>((width_ * height_ * min_fps *
                kKushGaugeConstant)/1000);
    average_bandwidth_ = static_cast<int>((max_bandwidth_+min_bandwidth_)/2);
}

QualityConfig::QualityConfig()
    : target_framerate_(0), target_bitrate_(0),
    packet_loss_(3 * 30), rtt_(3 * 30), average_qp_(3 * 30) {

    config_media_  = ConfigMediaSingleton::Instance();
    std::list <ConfigMedia::VideoResolution> resolution_list = 
        config_media_->GetVideoResolutionList();

    use_dynamic_resolution_  =  config_media_->GetVideoDynamicResolution();
    use_4_3_resolution_  =  config_media_->GetResolution4_3();

    // 4:3 resolution
    for(std::list<ConfigMedia::VideoResolution>::iterator iter =
            resolution_list.begin(); iter != resolution_list.end(); 
            iter++) {
        resolution_config_.push_back(
                ResolutionConfigEntry(iter->width_,iter->height_,20,kMaxFrameRate));
    }
}


QualityConfig::~QualityConfig(){
}


void QualityConfig::ReportQP(int qp){
    average_qp_.AddSample(qp);
    const rtc::Optional<int> avg_qp = average_qp_.GetAverage();
    if( avg_qp ){
        if (*avg_qp > kHighH264QpThreshold ) {
            adaptation_up_ = true;
        }
        else if (*avg_qp < kHighH264QpThreshold ) {
            adaptation_down_ = true;
        }
    }
}

void QualityConfig::ReportChannelParameters(uint32_t packet_loss, uint64_t rtt ){
    packet_loss_.AddSample(packet_loss);
    rtt_.AddSample(rtt);

    const rtc::Optional<int> packet_loss_avg = packet_loss_.GetAverage();
    if( packet_loss_avg ){
        if( *packet_loss_avg > kPacketLossThreshold ) {
            adaptation_down_ = true;
            return;
        }
    }

    const rtc::Optional<int> rtt_avg = rtt_.GetAverage();
    if( rtt_avg ){
        if( *rtt_avg > kRttMaxThreshold ) {
            adaptation_down_ = true;
            return;
        }
    }
}

void QualityConfig::ReportFrameRate(int framerate) {
    if( target_framerate_ == framerate ) return;
    RTC_LOG(INFO) << "FrameRate changed from " << target_framerate_ << ", to "
        << framerate;
    target_framerate_ = framerate;
}

void QualityConfig::ReportMaxBitrate(int bitrate) {
    RTC_LOG(INFO) << "Setting Max Bitrate : " << bitrate;
    max_bitrate_ = bitrate;
}

void QualityConfig::ReportTargetBitrate(int bitrate) {
    if( bitrate == target_bitrate_ ) return;
    // RTC_LOG(INFO) << "Bitrate changed from " << target_bitrate_
    //    << ", to " << bitrate ;
    target_bitrate_ = bitrate;
}

void QualityConfig::Reset() {
    packet_loss_.Reset();
    rtt_.Reset();
    average_qp_.Reset();
}

bool QualityConfig::IsAdaptationRequired() {
    return adaptation_up_ || adaptation_up_;
}

int QualityConfig::GetFrameRate(){
    if( target_framerate_ > kMaxFrameRate )
        return kMaxFrameRate;
    else
        return target_framerate_;
}

int QualityConfig::GetBitrate(){
    return target_bitrate_;
}

bool QualityConfig::GetBestMatch(QualityConfig::Resolution& resolution) {
    return GetBestMatch(target_bitrate_, resolution);
}

bool QualityConfig::GetInitialBestMatch(QualityConfig::Resolution& resolution) {
    Resolution candidate;

    // The initial resolution will be used
    // if the use default resolution flag is on.
    if( use_dynamic_resolution_ == false) {
        config_media_->GetFixedVideoResolution(candidate.width_, candidate.height_);
        candidate.framerate_ = kMaxFrameRate;
        candidate.bitrate_ = static_cast<int>(
                (candidate.width_ * candidate.height_ * kMaxFrameRate *
                kKushGaugeConstant * kMaxMontionFactor )/1000);
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
//
// TODO: Evaluation of this method is appropriate
// TODO: QP/LOSS/RTT
//       Currently, Google is working on a lot of BWE related work,
//       so it needs to be modified or implemented
//       according to the implementation status of WebRTC Native Package.
//
////////////////////////////////////////////////////////////////////////////////
bool QualityConfig::GetBestMatch(int target_bitrate,
        QualityConfig::Resolution& resolution) {
    Resolution candidate;
    int last_diff =  std::numeric_limits<int>::max();
    int diff = 0;

    target_bitrate_ = target_bitrate;

    if( use_dynamic_resolution_ == false ) {
        // The encoder does not use the Bitrate Estimation delivered by BWE,
        // but keeps the initially generated resolution.
        resolution = current_res_;
        return false;   // Do not change resoltuion
    };

    for( std::list<ResolutionConfigEntry>::iterator iter = resolution_config_.begin();
            iter != resolution_config_.end(); iter++) {
        diff = abs(iter->average_bandwidth_ - target_bitrate );
        if( last_diff > diff ) {
            candidate.width_ = iter->width_;
            candidate.height_ = iter->height_;
            candidate.bitrate_ = target_bitrate_;
            candidate.framerate_ = target_framerate_;
            last_diff = diff;
        }
    }

    if( candidate.width_ != current_res_.width_ ||
            candidate.height_ != current_res_.height_ ) {
        current_res_ = resolution = candidate;
        RTC_LOG(INFO) << "BestMatch Resolution for bitrate "
            << target_bitrate_ << " : "
            << candidate.width_ << "x" << candidate.height_ ;
        return true;
    }
    resolution = candidate;
    return false;
}

