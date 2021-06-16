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

#include "raspi_motion.h"

#include <limits>
#include <string>

#include "config_motion.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/thread.h"

namespace {

const float kKushGaugeConstant = 0.07;
const float kKushGaugeRank = 2;
const float kBufferCalculationFactor = 1.2;

const int kDefaultMotionAverageSize = 32;
const int kDefaultMotionActiveTriggerPercent = 10;
const int kDefaultMotionActiveClearPercent = 5;
const int kDefaultMotionClearWaitPeriod = 5000;   // 5 seconds
const int kDefaultMotionAveragePrintDiff = 1000;  // 1 second

#ifdef PRINT_PROCESS_DELAYS
const int kDefaultMotionFrameProcessingSize = 30 * 3;
#endif  // PRINT_PROCESS_DELAYS

}  // namespace

RaspiMotionHolder::RaspiMotionHolder(ConfigMotion *config_motion)
    : config_motion_(config_motion) {}

RaspiMotionHolder::~RaspiMotionHolder() {}

bool RaspiMotionHolder::Start() {
    if (config_motion_->GetDetectionEnable() == false) return false;
    RTC_LOG(INFO) << "RaspMotion Starting";
    RTC_LOG(INFO) << "RaspiMotion is Active: "
                  << (raspi_motion_ && raspi_motion_->IsActive());
    if (!raspi_motion_) {
        RTC_LOG(INFO) << "Starting RaspiMotion Detection";
        raspi_motion_.reset(new RaspiMotion(config_motion_));
        return raspi_motion_->StartCapture();
    }
    RTC_LOG(LS_ERROR) << "RaspiMotion is already running!";
    return false;
}

bool RaspiMotionHolder::Stop() {
    if (config_motion_->GetDetectionEnable() == false) return false;
    RTC_LOG(INFO) << "RaspMotion Stopping";
    RTC_LOG(INFO) << "RaspiMotion is Active: "
                  << (raspi_motion_ && raspi_motion_->IsActive());
    if (raspi_motion_ && raspi_motion_->IsActive() == true) {
        RTC_LOG(INFO) << "Stopping RaspiMotion Detection";
        raspi_motion_->StopCapture();
        raspi_motion_.reset();
        return true;
    }
    RTC_LOG(LS_ERROR) << "RaspiMotion is not running!";
    return false;
}

bool RaspiMotionHolder::SetNotificationUrl(bool enable, const std::string url) {
    return true;
}

RaspiMotion::RaspiMotion(ConfigMotion *config_motion, int width, int height,
                         int framerate, int bitrate)
    : Event(false, false),
      motion_active_(false),
      motion_drain_quit_(false),
      width_(width),
      height_(height),
      framerate_(framerate),
      bitrate_(bitrate),
      mmal_encoder_(nullptr),
      clock_(webrtc::Clock::GetRealTimeClock()),
      motion_analysis_(width, height, framerate, false,
                       config_motion->GetBlobCancelThreshold(),
                       config_motion->GetBlobTrackingThreshold()),
      motion_active_average_(kDefaultMotionAverageSize),
#ifdef PRINT_PROCESS_DELAYS
      imv_process_delay_(kDefaultMotionFrameProcessingSize),
      frame_process_delay_(kDefaultMotionFrameProcessingSize),
      drain_process_delay_(kDefaultMotionFrameProcessingSize),
#endif  // PRINT_PROCESS_DELAYS
      notification_enable_(false),
      config_motion_(config_motion),
      task_queue_factory_(webrtc::CreateDefaultTaskQueueFactory()),
      worker_queue_(task_queue_factory_->CreateTaskQueue(
          "Raspi Motion", webrtc::TaskQueueFactory::Priority::HIGH)) {
    frame_buffer_size_ =
        static_cast<int>(((width * height * framerate * kKushGaugeConstant *
                           kKushGaugeRank * VIDEO_INTRAFRAME_PERIOD) /
                          8 /* bits to bytes */) *
                         kBufferCalculationFactor);
    mv_buffer_size_ = ((width / 16 + 1) * (height / 16) * 4) *
                      VIDEO_INTRAFRAME_PERIOD * 30 /*fps*/;
    motion_clear_wait_period_ = config_motion->GetClearWaitPeriod();
    motion_active_percent_clear_threshold_ = config_motion->GetClearPercent();
    RTC_LOG(INFO) << "Frame Queue Size: " << frame_buffer_size_
                  << ", MV Queue Size: " << mv_buffer_size_;

    motion_file_.reset(new RaspiMotionFile(
        config_motion, config_motion->GetDirectory(),
        config_motion->GetFilePrefix(), frame_buffer_size_, mv_buffer_size_));

    motion_analysis_.SetBlobEnable(true);
    motion_analysis_.RegisterBlobObserver(this);
    motion_analysis_.RegisterImvObserver(this);

    motion_state_ = CLEARED;
    last_average_print_timestamp_ = 0;
    motion_clear_wait_period_ = kDefaultMotionClearWaitPeriod;

    motion_active_percent_trigger_threshold_ =
        kDefaultMotionActiveTriggerPercent;
    motion_active_percent_clear_threshold_ = kDefaultMotionActiveClearPercent;
}

RaspiMotion::RaspiMotion(ConfigMotion *config_motion)
    : RaspiMotion(config_motion, config_motion->GetWidth(),
                  config_motion->GetHeight(), config_motion->GetFps(),
                  config_motion->GetBitrate()) {}

bool RaspiMotion::IsActive() const { return motion_active_; }
bool RaspiMotion::IsEnabled() const {
    return config_motion_->GetDetectionEnable();
}

bool RaspiMotion::StartCapture() {
    RTC_LOG(INFO) << "Raspi Motion Starting";

    // Get the instance of MMAL encoder wrapper
    if ((mmal_encoder_ = webrtc::MMALWrapper::Instance()) == nullptr) {
        RTC_LOG(LS_ERROR) << "Failed to get MMAL encoder wrapper";
        return false;
    }

    wstreamer::EncoderSettings params;
    params.imv_enable = true;
    if (config_motion_->GetEnableAnnotateText()) {
        params.annotation_enable = config_motion_->GetEnableAnnotateText();
        params.annotation_text = config_motion_->GetAnnotateText();
        params.annotation_text_size = config_motion_->GetAnnotateTextSize();
    }

    // Set media config params at first.
    mmal_encoder_->SetEncoderConfigParams(&params);

    RTC_LOG(INFO) << "Motion Video Params: " << width_ << " x " << height_
                  << "@" << framerate_ << ", " << bitrate_ << " kbps";
    if (mmal_encoder_->InitEncoder(wstreamer::VideoEncodingParams(
            width_, height_, framerate_, bitrate_)) == false) {
        return false;
    }

    // start capture in here
    mmal_encoder_->StartCapture();

    // start drain thread ;
    if (drainThread_.empty()) {
        RTC_LOG(INFO) << "Frame drain thread initialized.";
        drainThread_ = rtc::PlatformThread::SpawnJoinable(
            [this] {
                while (DrainProcess()) {
                }
            },
            "DrainThread",
            rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));
    }

    motion_active_ = true;
    motion_drain_quit_ = false;
    return true;
}

void RaspiMotion::StopCapture() {
    motion_active_ = false;

    if (drainThread_.empty() == false) {
        webrtc::MutexLock lock(&drain_lock_);
        motion_drain_quit_ = true;
        drainThread_.Finalize();
    }

    if (mmal_encoder_) {
        mmal_encoder_->StopCapture();
        mmal_encoder_->UninitEncoder();
        mmal_encoder_ = nullptr;
    }
}

RaspiMotion::~RaspiMotion() {
    RTC_LOG(INFO) << "Raspi Motion Stopping";
    StopCapture();
}

///////////////////////////////////////////////////////////////////////////////
//
// Motion Observer and State management
//
///////////////////////////////////////////////////////////////////////////////
void RaspiMotion::OnMotionTriggered(int active_nums) {
    // Changing state to TRIGGERED
    if (motion_state_ == CLEARED) {
        RTC_LOG(INFO) << "Motion Changing state CLEAR to TRIGGERED. active num:"
                      << active_nums;
    } else if (motion_state_ == WAIT_CLEAR) {
        RTC_LOG(INFO)
            << "Motion Changing state WAIT_CLEAR to TRIGGERED. active num:"
            << active_nums;
    }
    motion_state_ = TRIGGERED;
}

void RaspiMotion::OnMotionCleared(int updates) {
    RTC_LOG(INFO) << "Motion Blob Deactivated. update count was: " << updates;
    // Changing state to WAIT_CLEAR
    if (motion_state_ == TRIGGERED) {
        RTC_LOG(INFO) << "Motion Changing state TRIGGERED to WAIT_CLEAR ";
        motion_clear_wait_timestamp_ = clock_->TimeInMilliseconds();
        motion_state_ = WAIT_CLEAR;
    } else if (motion_state_ == CLEARED) {
        RTC_LOG(INFO) << "Invalid Motion state changing CLEARED to WAIT_CLEAR ";
    }
}

void RaspiMotion::OnActivePoints(int total_points, int active_points) {
    double active_percent = active_points * 100 / total_points;
    uint64_t current_timestamp;

    motion_active_average_.AddSample((int)active_percent);
    absl::optional<int> moving_average =
        motion_active_average_.GetAverageRoundedDown();
    if (moving_average) {
        current_timestamp = clock_->TimeInMilliseconds();
        if (current_timestamp - last_average_print_timestamp_ >
            kDefaultMotionAveragePrintDiff) {
            last_average_print_timestamp_ = current_timestamp;
            if (motion_state_ == TRIGGERED || motion_state_ == WAIT_CLEAR) {
                RTC_LOG(INFO) << "Motion active percent:  " << *moving_average;
            };

#ifdef PRINT_PROCESS_DELAYS
            // printing average delay in processing
            absl::optional<int> frame_process_delay =
                frame_process_delay_.GetAverageRoundedDown();
            absl::optional<int> imv_process_delay =
                imv_process_delay_.GetAverageRoundedDown();
            absl::optional<int> drain_waiting_delay =
                drain_process_delay_.GetAverageRoundedDown();
            if (frame_process_delay) {
                RTC_LOG(INFO) << "Process Delay Frame: " << *frame_process_delay
                              << ", IMV: " << *imv_process_delay
                              << ", Drain(w/ wait): " << *drain_waiting_delay;
            }
#endif  // PRINT_PROCESS_DELAYS
        }
        if (motion_state_ == WAIT_CLEAR) {
            if (*moving_average < motion_active_percent_clear_threshold_ &&
                current_timestamp - motion_clear_wait_timestamp_ >
                    motion_clear_wait_period_) {
                RTC_LOG(INFO) << "Motion Changing state WAIT_CLEAR to CLEAR ";
                motion_state_ = CLEARED;
            }
        }
    };
}

///////////////////////////////////////////////////////////////////////////////
//
// Raspi Encoder frame drain processing
//
///////////////////////////////////////////////////////////////////////////////
#ifdef PRINT_PROCESS_DELAYS
#define __CLOCK_MARK_START__ start_time = clock_->TimeInMicroseconds();
#define __CLOCK_MARK_FRAME_END__              \
    mark_time = clock_->TimeInMicroseconds(); \
    frame_process_delay_.AddSample(mark_time - start_time);

#define __CLOCK_MARK_IMV_END__                \
    mark_time = clock_->TimeInMicroseconds(); \
    imv_process_delay_.AddSample(mark_time - start_time);

#define __CLOCK_MARK_DRAIN_END__              \
    mark_time = clock_->TimeInMicroseconds(); \
    drain_process_delay_.AddSample(mark_time - start_time);
#else
#define __CLOCK_MARK_START__
#define __CLOCK_MARK_FRAME_END__
#define __CLOCK_MARK_IMV_END__
#define __CLOCK_MARK_DRAIN_END__
#endif  // PRINT_PROCESS_DELAYS

bool RaspiMotion::DrainProcess() {
#ifdef PRINT_PROCESS_DELAYS
    uint64_t start_time, mark_time;
#endif  // PRINT_PROCESS_DELAYS
    webrtc::FrameBuffer *buf = nullptr;
    size_t length;

    if (motion_drain_quit_ == true) {
        // need to stop Wwriter thread at this drain process before quit this
        // because the writer thread created in this drain process
        if (motion_file_->IsWriterActive() == true) {
            RTC_LOG(INFO) << "Stopping motion video writer thread";
            motion_file_->StopWriter();
        }
        return false;
    };

    __CLOCK_MARK_START__;
    buf = mmal_encoder_->ReadFront();
    __CLOCK_MARK_DRAIN_END__;
    if (buf && buf->length() > 0) {
        __CLOCK_MARK_START__;

        bool is_keyframe = buf->isKeyFrame();

        if (buf->isMotionVector()) {
            // queuing the motion vector for file writer
            if (motion_file_->ImvQueuing(buf->data(), buf->length(), &length,
                                         is_keyframe) == false) {
                RTC_LOG(LS_ERROR) << "Failed to WriteBack in MV queue ";
            };

            // Doing motion analysis based on the new MV
            motion_analysis_.Analyse(buf->data(), buf->length());
            __CLOCK_MARK_IMV_END__;

            if ((motion_state_ == CLEARED) &&
                (motion_file_->IsWriterActive() == true)) {
                motion_file_->StopWriter();
                // After the file writer ends, old files are deleted so that the
                // directory where the file is stored does not exceed the
                // specified size limit.
                worker_queue_.PostTask(std::make_unique<LimitTotalDirSizeTask>(
                    config_motion_->GetDirectory(),
                    config_motion_->GetFilePrefix(),
                    (size_t)config_motion_->GetTotalFileSizeLimit() * 1000000));

            } else if ((motion_state_ == TRIGGERED) &&
                       (motion_file_->IsWriterActive() == false)) {
                motion_file_->StartWriter();
            }

        } else if (buf->isFrameEnd()) {
            if (motion_file_->FrameQueuing(buf->data(), buf->length(), &length,
                                           is_keyframe) == false) {
                RTC_LOG(LS_ERROR) << "Failed to WriteBack in frame buffer ";
            };
            RTC_DCHECK(buf->length() == length)
                << "Failed to FrameQueueing buffer: " << buf->length()
                << ", length: " << length;
            __CLOCK_MARK_FRAME_END__;

        } else {
            RTC_LOG(LS_ERROR) << "FrameBuffer is not frame nor motionvector : "
                              << buf->toString();
        }
    }
    return true;
}

