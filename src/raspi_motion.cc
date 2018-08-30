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


#include <limits>
#include <string>

#include "common_types.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/platform_thread.h"

#include "mmal_video.h"

#include "raspi_motion.h"
#include "config_media.h"
#include "config_motion.h"

static const float  kKushGaugeConstant = 0.07;
int  RaspiMotion::kEventWaitPeriod = 20;    // minimal wait ms period between frame

static const int kDefaultMotionAverageSize = 32;
static const int kDefaultMotionActiveTriggerPercent = 10;
static const int kDefaultMotionActiveClearPercent = 5;
static const int kDefaultMotionClearWaitPeriod = 5000; // 5 seconds

static const int kDefaultMotionAveragePrintDiff = 1000; // 1 second


RaspiMotion::RaspiMotion(int width, int height, int framerate, int bitrate)
    :   Event(false,false), is_active_(false),
    width_(width), height_(height), framerate_(framerate), bitrate_(bitrate),
    mmal_encoder_(nullptr),clock_(webrtc::Clock::GetRealTimeClock()),
    motion_(width, height, framerate, false),
    motion_active_average_(kDefaultMotionAverageSize) {

    queue_capacity_ = framerate * VIDEO_INTRAFRAME_PERIOD +
        (framerate * VIDEO_INTRAFRAME_PERIOD) *0.1f;
    frame_queue_size_ = (width * height * kKushGaugeConstant * 2)/8;
    mv_queue_size_ = (width/16+1) * (height/16) * 4;

    mv_shared_buffer_.reset( new rtc::BufferQueue(queue_capacity_, mv_queue_size_ ));

    motion_file_.reset( new RaspiMotionFile(config_motion::motion_directory,
                config_motion::motion_file_prefix,
                queue_capacity_, frame_queue_size_, mv_queue_size_ ));

    motion_.SetBlobEnable(true);
    motion_.RegisterBlobObserver(this);
    motion_.RegisterImvObserver(this);

    motion_state_  = CLEARED;
    last_average_print_timestamp_ = 0;
    motion_clear_wait_period_ = kDefaultMotionClearWaitPeriod;

    motion_active_percent_trigger_threshold_ = kDefaultMotionActiveTriggerPercent;
    motion_active_percent_clear_threshold_ = kDefaultMotionActiveClearPercent;
}

RaspiMotion::RaspiMotion() :
    RaspiMotion(config_motion::motion_width, config_motion::motion_height,
            config_motion::motion_fps,config_motion::motion_bitrate) {
    motion_clear_wait_period_ = config_motion::motion_clear_wait_period;
    motion_active_percent_clear_threshold_ = config_motion::motion_clear_percent;
}

bool RaspiMotion::IsActive() const {
    return is_active_;
}

bool RaspiMotion::StartCapture() {
    RTC_LOG(INFO) << "Raspi Motion Starting";
    // media configuration sigleton reference
    ConfigMedia *config_media = ConfigMediaSingleton::Instance();

    // Get the instance of MMAL encoder wrapper
    if( (mmal_encoder_ = webrtc::MMALWrapper::Instance() ) == nullptr ) {
        RTC_LOG(LS_ERROR) << "Failed to get MMAL encoder wrapper";
        return false;
    }

    // Enable InlineMotionVectors
    mmal_encoder_->SetInlineMotionVectors(true);

    // Setting Intra Frame period
    mmal_encoder_->SetIntraPeriod(framerate_ * VIDEO_INTRAFRAME_PERIOD);

    // Setting Video Rotation and Flip setting
    mmal_encoder_->SetVideoRotation(config_media->GetVideoRotation());
    mmal_encoder_->SetVideoFlip(config_media->GetVideoVFlip(), 
            config_media->GetVideoHFlip());
    mmal_encoder_->SetVideoAnnotate(config_motion::motion_enable_annotate_text);
    if( config_motion::motion_enable_annotate_text == true ){
        mmal_encoder_->SetVideoAnnotateUserText(config_motion::motion_annotate_text);
        mmal_encoder_->SetVideoAnnotateTextSize(config_motion::motion_annotate_text_size );
    };

    // clear Annotation text size ratio value
    mmal_encoder_->SetVideoAnnotateTextSizeRatio(0);

    // Video Image related parameter settings
    mmal_encoder_->SetVideoSharpness(config_media->GetVideoSharpness());
    mmal_encoder_->SetVideoContrast(config_media->GetVideoContrast());
    mmal_encoder_->SetVideoBrightness(config_media->GetVideoBrightness());
    mmal_encoder_->SetVideoSaturation(config_media->GetVideoSaturation());
    mmal_encoder_->SetVideoEV(config_media->GetVideoEV());
    mmal_encoder_->SetVideoExposureMode(config_media->GetVideoExposureMode());
    mmal_encoder_->SetVideoFlickerMode(config_media->GetVideoFlickerMode());
    mmal_encoder_->SetVideoAwbMode(config_media->GetVideoAwbMode());
    mmal_encoder_->SetVideoDrcMode(config_media->GetVideoDrcMode());

    RTC_LOG(INFO) << "Initial Motion Video : " << width_ << " x " << height_
        << "@" << framerate_ << ", " << bitrate_ << " kbps";
    if(mmal_encoder_->InitEncoder(width_, height_, framerate_, bitrate_ ) == false ){
        return false;
    }

    // start capture in here
    mmal_encoder_->StartCapture();

    // start drain thread ;
    if ( !drainThread_) {
        RTC_LOG(INFO) << "Frame drain thread initialized.";
        drainThread_.reset(new rtc::PlatformThread(
                               RaspiMotion::DrainThread, this, "FrameDrain"));
        drainThread_->Start();
        drainThread_->SetPriority(rtc::kHighPriority);
        drainThreadStarted_ = true;
    }

    // start Motion Vector thread ;
    if ( !motionVectorThread_) {
        RTC_LOG(INFO) << "Motion Vector analyse thread initialized.";
        motionVectorThread_.reset(new rtc::PlatformThread(
                               RaspiMotion::MotionVectorThread, this, "MotionVector"));
        motionVectorThread_->Start();
        motionVectorThread_->SetPriority(rtc::kHighPriority);
        motionVectorThreadStarted_ = true;
    }

    is_active_ = true;
    return true;
}

void RaspiMotion::StopCapture() {
    is_active_ = false;

    if (motionVectorThread_) {
        motionVectorThreadStarted_ = false;
        motionVectorThread_->Stop();
        motionVectorThread_.reset();
    }

    if (drainThread_) {
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
void RaspiMotion::OnMotionTriggered(int active_nums)  {
    // Changing state to TRIGGERED
    if( motion_state_ == CLEARED ) {
        RTC_LOG(INFO) << "Motion Changing state CLEAR to TRIGGERED. active num:"
            << active_nums;
        // motion_file_->StartWriterThread();
    }
    else if( motion_state_ == WAIT_CLEAR ) {
        RTC_LOG(INFO) << "Motion Changing state WAIT_CLEAR to TRIGGERED. active num:"
            << active_nums;
    }
    motion_state_ = TRIGGERED;
}

void RaspiMotion::OnMotionCleared(int updates) {
    RTC_LOG(INFO) << "Motion Blob Deactivated. update count was: " << updates;
    // Changing state to WAIT_CLEAR
    if( motion_state_ == TRIGGERED ) {
        RTC_LOG(INFO) << "Motion Changing state TRIGGERED to WAIT_CLEAR ";
        motion_clear_wait_timestamp_ = clock_->TimeInMilliseconds();
        motion_state_ = WAIT_CLEAR;
    }
    else if( motion_state_ == CLEARED ) {
        RTC_LOG(INFO) << "Invalid Motion state changing CLEARED to WAIT_CLEAR ";
    }
}

void RaspiMotion::OnActivePoints(int total_points, int active_points){
    double active_percent = active_points*100/total_points;
    uint64_t current_timestamp;

    motion_active_average_.AddSample( (int)active_percent );
    absl::optional<int> moving_average = motion_active_average_.GetAverage();
    if( moving_average ){
        current_timestamp = clock_->TimeInMilliseconds();
        if( current_timestamp - last_average_print_timestamp_
                > kDefaultMotionAveragePrintDiff ) {
            last_average_print_timestamp_ = current_timestamp;
            if( motion_state_ == TRIGGERED ||
                    motion_state_ == WAIT_CLEAR ) {
                RTC_LOG(INFO) << "Motion active percent:  " << *moving_average;
            };
        }
        if( motion_state_ == WAIT_CLEAR ) {
            if( *moving_average <  motion_active_percent_clear_threshold_ &&
                current_timestamp - motion_clear_wait_timestamp_ > motion_clear_wait_period_  )  {
                RTC_LOG(INFO) << "Motion Changing state WAIT_CLEAR to CLEAR ";
                // motion_file_->StopWriterThread();
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
bool RaspiMotion::DrainThread(void* obj) {
    return static_cast<RaspiMotion *> (obj)->DrainProcess();
}

bool RaspiMotion::DrainProcess() {
    MMAL_BUFFER_HEADER_T *buf = nullptr;
    size_t length;

    buf = mmal_encoder_->GetEncodedFrame();
    if ( buf && buf->length > 0 ) {
        bool is_keyframe =  buf->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME;
        if( is_keyframe && motion_file_->FrameQueueSize() != 0 ) {
            // Reset the frame and motion vector queue
            motion_file_->QueueClear();
        };

        if( buf->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO ) {
            // queuing the motion vector for file writer
            if( motion_file_->ImvQueuing(buf->data, buf->length, &length, is_keyframe)
                    == false ) {
                RTC_LOG(LS_ERROR) << "Failed to WriteBack in MV queue ";
            };

            // queuing motion vector for motion analysis
            if( mv_shared_buffer_->WriteBack(buf->data, buf->length, &length)
                    == false ) {
                RTC_LOG(LS_ERROR) << "Faild to queue in MV shared buffer";
                Set(); // Event Set to wake up
            };
        }
        else {
            // queuing video frame for file writing
            if( motion_file_->FrameQueuing(buf->data, buf->length, &length, is_keyframe)
                    == false ) {
                RTC_LOG(LS_ERROR) << "Failed to WriteBack in frame queue ";
            };
        }
    }

    if( buf ) mmal_encoder_->ReleaseFrame(buf);
    // TODO: if encoded_size is zero, we need to reset encoder itself
    return true;
}


///////////////////////////////////////////////////////////////////////////////
//
// Raspi motion vector processing
//
///////////////////////////////////////////////////////////////////////////////
bool RaspiMotion::MotionVectorThread(void* obj) {
    return static_cast<RaspiMotion *> (obj)->MotionVectorProcess();
}

bool RaspiMotion::MotionVectorProcess() {
    uint64_t begin_milli_timestamp, end_milli_timestamp;
    uint8_t  buffer[mv_queue_size_];
    size_t   bytes;

    if( mv_shared_buffer_->size() == 0 )
        Wait(kEventWaitPeriod);    // Waiting for Event or Timeout

    if( mv_shared_buffer_->ReadFront(buffer, mv_queue_size_, &bytes ) ) {
        RTC_DCHECK( bytes == mv_queue_size_ ) << "Error in Motion Vector size";
        begin_milli_timestamp = clock_->TimeInMilliseconds();
        motion_.Analyse( buffer, bytes );
        end_milli_timestamp = clock_->TimeInMilliseconds();

        if( (motion_state_ == CLEARED) &&
                (motion_file_->WriterStatus() == true) ) {
                motion_file_->StopWriterThread();

                //
                motion_file_->ManagingVideoFolder();
        }
        else if( (motion_state_ == TRIGGERED) &&
                (motion_file_->WriterStatus() == false) ) {
                motion_file_->StartWriterThread();
        }

    }
    return true;
}



