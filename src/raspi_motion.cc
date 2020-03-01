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

#include "common_types.h"
#include "config_motion.h"
#include "mmal_video.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/thread.h"

static const float kKushGaugeConstant = 0.07;
static const uint64_t kDrainProcessDelayMaximumInMicro = 32000;  // 32 ms
static const int kEventWaitPeriod = 5;

static const int kDefaultMotionAverageSize = 32;
static const int kDefaultMotionActiveTriggerPercent = 10;
static const int kDefaultMotionActiveClearPercent = 5;
static const int kDefaultMotionClearWaitPeriod = 5000;   // 5 seconds
static const int kDefaultMotionAveragePrintDiff = 1000;  // 1 second

RaspiMotion::RaspiMotion(int width, int height, int framerate, int bitrate)
    : Event(false, false),
      motion_active_(false),
      motion_drain_quit_(false),
      motion_vector_quit_(false),
      width_(width),
      height_(height),
      framerate_(framerate),
      bitrate_(bitrate),
      mmal_encoder_(nullptr),
      clock_(webrtc::Clock::GetRealTimeClock()),
      motion_analysis_(width, height, framerate, false),
      motion_active_average_(kDefaultMotionAverageSize) {
    queue_capacity_ = (framerate * VIDEO_INTRAFRAME_PERIOD * 2) * 1.2;
    frame_queue_size_ = (width * height * kKushGaugeConstant * 2) / 8;
    mv_queue_size_ = (width / 16 + 1) * (height / 16) * 4;

    mv_shared_buffer_.reset(
        new rtc::BufferQueue(queue_capacity_, mv_queue_size_));

    motion_file_.reset(new RaspiMotionFile(
        config_motion::motion_directory, config_motion::motion_file_prefix,
        queue_capacity_, frame_queue_size_, mv_queue_size_));

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

RaspiMotion::RaspiMotion()
    : RaspiMotion(config_motion::motion_width, config_motion::motion_height,
                  config_motion::motion_fps, config_motion::motion_bitrate) {
    motion_clear_wait_period_ = config_motion::motion_clear_wait_period;
    motion_active_percent_clear_threshold_ =
        config_motion::motion_clear_percent;
}

bool RaspiMotion::IsActive() const { return motion_active_; }

bool RaspiMotion::StartCapture() {
    RTC_LOG(INFO) << "Raspi Motion Starting";

    // Get the instance of MMAL encoder wrapper
    if ((mmal_encoder_ = webrtc::MMALWrapper::Instance()) == nullptr) {
        RTC_LOG(LS_ERROR) << "Failed to get MMAL encoder wrapper";
        return false;
    }

    // Set media config params at first.
    mmal_encoder_->SetMediaConfigParams();

    //
    // Overriding media config params for Motion Video
    //
    // Enable InlineMotionVectors
    mmal_encoder_->SetInlineMotionVectors(true);

    // Setting Intra Frame period
    mmal_encoder_->SetIntraPeriod(framerate_ * VIDEO_INTRAFRAME_PERIOD);

    mmal_encoder_->SetVideoAnnotate(config_motion::motion_enable_annotate_text);
    if (config_motion::motion_enable_annotate_text == true) {
        mmal_encoder_->SetVideoAnnotateUserText(
            config_motion::motion_annotate_text);
        mmal_encoder_->SetVideoAnnotateTextSize(
            config_motion::motion_annotate_text_size);
    };

    RTC_LOG(INFO) << "Initial Motion Video : " << width_ << " x " << height_
                  << "@" << framerate_ << ", " << bitrate_ << " kbps";
    if (mmal_encoder_->InitEncoder(width_, height_, framerate_, bitrate_) ==
        false) {
        return false;
    }

    // start capture in here
    mmal_encoder_->StartCapture();

    // start drain thread ;
    if (!drainThread_) {
        RTC_LOG(INFO) << "Frame drain thread initialized.";
        drainThread_.reset(new rtc::PlatformThread(
            RaspiMotion::DrainThread, this, "FrameDrain", rtc::kHighPriority));
        drainThread_->Start();
        drainThreadStarted_ = true;
    }

    // start Motion Vector thread ;
    if (!motionVectorThread_) {
        RTC_LOG(INFO) << "Motion Vector analyse thread initialized.";
        motionVectorThread_.reset(
            new rtc::PlatformThread(RaspiMotion::MotionVectorThread, this,
                                    "MotionVector", rtc::kHighPriority));
        motionVectorThread_->Start();
        motionVectorThreadStarted_ = true;
    }

    motion_active_ = true;
    return true;
}

void RaspiMotion::StopCapture() {
    motion_active_ = false;

    if (motionVectorThread_) {
        motion_vector_quit_ = true;
        motionVectorThreadStarted_ = false;
        motionVectorThread_->Stop();
        motionVectorThread_.reset();
    }

    if (drainThread_) {
        motion_drain_quit_ = true;
        drainThreadStarted_ = false;
        drainThread_->Stop();
        drainThread_.reset();
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
        // motion_file_->StartWriterThread();
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
void RaspiMotion::DrainThread(void *obj) {
    RaspiMotion *raspi_motion = static_cast<RaspiMotion *>(obj);
    while (raspi_motion->DrainProcess()) {
    }
}

bool RaspiMotion::DrainProcess() {
    uint64_t current_timestamp, timestamp;
    MMAL_BUFFER_HEADER_T *buf = nullptr;
    size_t length;

    if (motion_drain_quit_ == true) {
        return false;
    };

    current_timestamp = clock_->TimeInMicroseconds();
    buf = mmal_encoder_->GetEncodedFrame();
    if (buf && buf->length > 0) {
        bool is_keyframe = buf->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME;

        if (buf->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) {
            // queuing the motion vector for file writer
            if (motion_file_->ImvQueuing(buf->data, buf->length, &length,
                                         is_keyframe) == false) {
                RTC_LOG(LS_ERROR) << "Failed to WriteBack in MV queue ";
            };

            // queuing motion vector for motion analysis
            if (mv_shared_buffer_->WriteBack(buf->data, buf->length, &length) ==
                false) {
                RTC_LOG(LS_ERROR) << "Faild to queue in MV shared buffer";
            };
            Set();  // Event Set to wake up motion vector process
        } else if (buf->flags & MMAL_BUFFER_HEADER_FLAG_FRAME) {
            if (motion_file_->FrameQueuing(buf->data, buf->length, &length,
                                           is_keyframe) == false) {
                RTC_LOG(LS_ERROR) << "Failed to WriteBack in frame queue ";
            };
            RTC_DCHECK(buf->length == length);
        } else {
            RTC_LOG(LS_ERROR) << "**************************************";
            dump_mmal_buffer(0, buf);
            RTC_LOG(LS_ERROR) << "**************************************";
        }
    }

    timestamp = clock_->TimeInMicroseconds();
    if (timestamp - current_timestamp > kDrainProcessDelayMaximumInMicro)
        RTC_LOG(LS_ERROR) << "Frame DrainProcess Time : "
                          << timestamp - current_timestamp;
    if (buf) mmal_encoder_->ReleaseFrame(buf);
    // TODO: if encoded_size is zero, we need to reset encoder itself
    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// Raspi motion vector processing
//
///////////////////////////////////////////////////////////////////////////////
void RaspiMotion::MotionVectorThread(void *obj) {
    RaspiMotion *raspi_motion = static_cast<RaspiMotion *>(obj);
    while (raspi_motion->MotionVectorProcess()) {
    }
}

bool RaspiMotion::MotionVectorProcess() {
    uint8_t buffer[mv_queue_size_];
    size_t bytes;

    if (motion_vector_quit_ == true) {
        return false;
    };

    if (mv_shared_buffer_->size() == 0)
        Wait(kEventWaitPeriod);  // Waiting for Event or Timeout

    if (mv_shared_buffer_->ReadFront(buffer, mv_queue_size_, &bytes)) {
        RTC_DCHECK(bytes == mv_queue_size_) << "Error in Motion Vector size";
        motion_analysis_.Analyse(buffer, bytes);

        if ((motion_state_ == CLEARED) &&
            (motion_file_->WriterActive() == true)) {
            motion_file_->StopWriter();

            //
            motion_file_->ManagingVideoFolder();
        } else if ((motion_state_ == TRIGGERED) &&
                   (motion_file_->WriterActive() == false)) {
            motion_file_->StartWriter();
        }
    }
    return true;
}
