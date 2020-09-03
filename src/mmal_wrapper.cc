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

#include "mmal_wrapper.h"

#include <stdio.h>
#include <string.h>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

////////////////////////////////////////////////////////////////////////////////////////
//
// Frame Queue
//
////////////////////////////////////////////////////////////////////////////////////////
int FrameQueue::kEventWaitPeriod = 30;  // minimal wait period between frame

FrameQueue::FrameQueue()
    : Event(false, false),
      num_(FRAME_QUEUE_LENGTH),
      size_(FRAME_BUFFER_SIZE),
      encoded_frame_queue_(nullptr),
      pool_internal_(nullptr),
      keyframe_buf_(nullptr),
      frame_buf_(nullptr),
      frame_buf_pos_(0),
      frame_segment_cnt_(0),
      inited_(false),
      frame_count_(0),
      frame_drop_(0),
      frame_queue_drop_(0) {}

FrameQueue::~FrameQueue() {
    mmal_queue_destroy(encoded_frame_queue_);
    mmal_pool_destroy(pool_internal_);
    delete frame_buf_;
}

bool FrameQueue::Isinitialized() { return inited_; }

bool FrameQueue::Init() {
    if (inited_ == true) return true;
    if ((pool_internal_ = mmal_pool_create(num_, size_)) == NULL) {
        RTC_LOG(LS_ERROR) << "FrameQueue internal pool creation failed";
        return false;
    };

    if ((encoded_frame_queue_ = mmal_queue_create()) == NULL) {
        RTC_LOG(LS_ERROR) << "FrameQueue queue creation failed";
        return false;
    };

    // temporary buffer to assemble frame from encoder
    frame_buf_ = new uint8_t[size_];
    inited_ = true;

    return true;
}

void FrameQueue::Reset() {
    MMAL_BUFFER_HEADER_T *buf;
    int len = mmal_queue_length(encoded_frame_queue_);

    for (int i = 0; i < len; i++) {
        buf = mmal_queue_get(encoded_frame_queue_);
        mmal_buffer_header_release(buf);
    };
    frame_buf_pos_ = 0;
    frame_segment_cnt_ = 0;
    inited_ = true;

    RTC_DCHECK_EQ(mmal_queue_length(encoded_frame_queue_), 0);
    RTC_DCHECK_EQ(mmal_queue_length(pool_internal_->queue), FRAME_QUEUE_LENGTH);
}

int FrameQueue::Length() { return mmal_queue_length(encoded_frame_queue_); }

void FrameQueue::ReleaseFrame(MMAL_BUFFER_HEADER_T *buffer) {
    RTC_DCHECK(inited_);
    mmal_buffer_header_release(buffer);
}

MMAL_BUFFER_HEADER_T *FrameQueue::GetEncodedFrame() {
    RTC_DCHECK(inited_);
    if (mmal_queue_length(encoded_frame_queue_) == 0)
        Wait(kEventWaitPeriod);  // Waiting for Event or Timeout

    if (mmal_queue_length(encoded_frame_queue_) == 0) return nullptr;
    return mmal_queue_get(encoded_frame_queue_);
}

void FrameQueue::HandlingMMALFrame(MMAL_BUFFER_HEADER_T *buffer) {
    RTC_DCHECK(inited_);
    RTC_DCHECK(buffer != NULL);
    RTC_DCHECK_GE((mmal_queue_length(encoded_frame_queue_) +
                   mmal_queue_length(pool_internal_->queue)),
                  FRAME_QUEUE_LENGTH - 1);

    // it should be same as FRAME_QUEUE_LENGTH except one frame buffer of pool
    // in the encoder there is something in the buffer
    if (buffer->length < FRAME_BUFFER_SIZE && buffer->length > 0) {
        // there is no end of frame mark in this buffer,
        // so keep it in the internal buffer
        if (!(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) ||
            (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)) {
            RTC_DCHECK(frame_buf_pos_ < size_);
            mmal_buffer_header_mem_lock(buffer);
            // save partial frame data to assemble frame at next time
            memcpy(frame_buf_ + frame_buf_pos_, buffer->data, buffer->length);
            frame_buf_pos_ += buffer->length;
            frame_segment_cnt_++;
            mmal_buffer_header_mem_unlock(buffer);
        }
        // end of frame marked
        else if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END ||
                 buffer->flags &
                     MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) {  // Motion Vector
            if ((int)(frame_buf_pos_ + buffer->length) >= size_) {
                RTC_LOG(INFO) << "frame_buf_pos : " << frame_buf_pos_
                              << ", buffer length: " << buffer->length;
            };
            RTC_DCHECK((int)(frame_buf_pos_ + buffer->length) < size_);

            // getting one frame buffer from pool
            MMAL_BUFFER_HEADER_T *frame = mmal_queue_get(pool_internal_->queue);
            if (frame) {  // frame buffer is available
                mmal_buffer_header_mem_lock(buffer);
                // copy the previously saved frame at first
                if (frame_buf_pos_) {
                    memcpy(frame->data, frame_buf_, frame_buf_pos_);
                }
                memcpy(frame->data + frame_buf_pos_, buffer->data,
                       buffer->length);

                // copying meta data to frame
                mmal_buffer_header_copy_header(frame, buffer);
                if (frame_buf_pos_) {
                    frame->length += frame_buf_pos_;
                }

                mmal_buffer_header_mem_unlock(buffer);

                // reset internal buffer pointer
                frame_segment_cnt_ = frame_buf_pos_ = 0;
                // one frame done
                mmal_queue_put(encoded_frame_queue_, frame);

                Set();  // Event set to wake up DrainThread
            } else {
                frame_drop_++;
                RTC_LOG(INFO) << "No availabe frame buffer in queue pool, "
                                 "frame dropped!: "
                              << frame_drop_;
            }
        } else {
            char buffer_log[256];
            dump_buffer_flag(buffer_log, 256, buffer->flags);
            RTC_LOG(INFO) << "**** MMAL Frame : " << buffer_log;
        }

        // frame count statistics
        frame_count_++;
    } else {
        RTC_LOG(LS_ERROR) << "**** MMAL Frame size error (buffer size: "
                          << FRAME_BUFFER_SIZE
                          << "), real frame size : " << buffer->length;
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// MMAL Encoder Delayed Reinit
//
////////////////////////////////////////////////////////////////////////////////////////
static const int kDelayInitialDurationMs = 4000;
static const int kDelayTaskInterval = 100;

///////////////////////////////////////////////////////////////////////////////
//
// Delayed Resolution Changing in HW Encoder
//
// In the case of MMAL HW Encoder, changing the resolution of the Encoder does
// not means that simply change the setting of Encoder,
// but it renders the MMAL Comonent destruction, creation and making link
// together so it gives a considerable delay and load.
//
// The WebRTC native code performs BWE at the same time as InitEncode.
// When changing the HW encoder setting, the delay and load affect the initial
// bandwidth decision of BWE. Therefore, after 2 seconds after InitEncode,
// it is necessary to reset the resolution according to the BWE bitrate.
//
///////////////////////////////////////////////////////////////////////////////

class EncoderDelayedInit::DelayInitTask : public webrtc::QueuedTask {
   public:
    explicit DelayInitTask(EncoderDelayedInit *encoder_delay_init_)
        : encoder_delay_init_(encoder_delay_init_) {
        RTC_LOG(LS_INFO)
            << "Created EncoderDelayedInit Task, Scheduling on queue...";
        webrtc::TaskQueueBase::Current()->PostDelayedTask(
            std::unique_ptr<webrtc::QueuedTask>(this), kDelayInitialDurationMs);
    }
    void Stop() {
        RTC_LOG(LS_INFO) << "Stopping DelayInitTask task.";
        stop_ = true;
    }

   private:
    bool Run() override {
        if (stop_) return true;  // TaskQueue will free this task.

        // RTC_LOG(INFO) << "EncoderDelayedInit Status " <<
        // encoder_delay_init_->status_;
        encoder_delay_init_->UpdateStatus();
        webrtc::TaskQueueBase::Current()->PostDelayedTask(
            std::unique_ptr<webrtc::QueuedTask>(this), kDelayTaskInterval);
        return false;  // Retain the task in order to reuse it.
    }

    EncoderDelayedInit *const encoder_delay_init_;
    bool stop_ = false;
};

EncoderDelayedInit::EncoderDelayedInit(MMALEncoderWrapper *mmal_encoder)
    : clock_(Clock::GetRealTimeClock()),
      last_init_timestamp_ms_(clock_->TimeInMilliseconds()),
      status_(INIT_PASS),
      mmal_encoder_(mmal_encoder),
      delayinit_task_(nullptr) {}

EncoderDelayedInit::~EncoderDelayedInit() {
    if (delayinit_task_) delayinit_task_->Stop();
}

bool EncoderDelayedInit::InitEncoder(int width, int height, int framerate,
                                     int bitrate) {
    if (mmal_encoder_->IsInited()) {
        RTC_LOG(LS_ERROR) << "MMAL Encoder already initialized.";
        return true;
    };

    RTC_LOG(INFO) << "InitEncoder " << width << "x" << height << "@"
                  << framerate << ", " << bitrate << " kbps";
    delayinit_task_ = new EncoderDelayedInit::DelayInitTask(this);

    // InitEncoder does not need to do any init delay
    RTC_LOG(INFO) << "EncoderDelay Status changed from INIT_PASS to WAITING";
    last_init_timestamp_ms_ = clock_->TimeInMilliseconds();
    status_ = INIT_WAITING;
    return mmal_encoder_->InitEncoder(width, height, framerate, bitrate);
}

bool EncoderDelayedInit::ReinitEncoder(int width, int height, int framerate,
                                       int bitrate) {
    if (mmal_encoder_->IsInited() == false) {
        RTC_LOG(LS_ERROR) << "MMAL Encoder does not initialized.";
        return false;
    };
    RTC_LOG(INFO) << "ReinitEncoder " << width << "x" << height << "@"
                  << framerate << ", " << bitrate << " kbps";

    if (status_ == INIT_PASS) {
        last_init_timestamp_ms_ = clock_->TimeInMilliseconds();
        status_ = INIT_WAITING;
        RTC_LOG(INFO)
            << "EncoderDelay Status changed from INIT_PASS to WAITING";
        if (mmal_encoder_->ReinitEncoder(width, height, framerate, bitrate) ==
            false) {
            RTC_LOG(LS_ERROR) << "Failed to reinitialize MMAL encoder";
            return false;
        };
        // start capture in here
        mmal_encoder_->StartCapture();
        return true;
    } else if (status_ == INIT_DELAY) {
        mmal_encoder_->SetEncodingParams(width, height, framerate, bitrate);
    } else if (status_ == INIT_WAITING) {
        if (mmal_encoder_->SetEncodingParams(width, height, framerate,
                                             bitrate)) {
            last_init_timestamp_ms_ = clock_->TimeInMilliseconds();
            status_ = INIT_DELAY;
            RTC_LOG(INFO)
                << "EncoderDelay Status changed from WAITING to DELAY";
        }
    }

    return true;
}

bool EncoderDelayedInit::UpdateStatus() {
    uint64_t timestamp_diff =
        clock_->TimeInMilliseconds() - last_init_timestamp_ms_;

    if (status_ == INIT_DELAY) {
        if (timestamp_diff > kDelayInitialDurationMs) {
            status_ = INIT_WAITING;
            RTC_LOG(INFO)
                << "EncoderDelay Status changed from DELAY to WAITING";
            if (mmal_encoder_->ReinitEncoderInternal() == false) {
                RTC_LOG(LS_ERROR) << "Failed to reinitialize MMAL encoder";
                return false;
            }
            // start capture in here
            mmal_encoder_->StartCapture();
        };
    } else if (status_ == INIT_WAITING) {
        if (timestamp_diff > kDelayInitialDurationMs) {
            RTC_LOG(INFO) << "EncoderDelay Status changed from WAIT to PASS";
            status_ = INIT_PASS;
        };
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
//
// MMAL Encoder Wrapper
//
////////////////////////////////////////////////////////////////////////////////////////

MMALEncoderWrapper::MMALEncoderWrapper()
    : encoder_initdelay_(this),
      mmal_initialized_(false),
      camera_preview_port_(nullptr),
      camera_video_port_(nullptr),
      camera_still_port_(nullptr),
      preview_input_port_(nullptr),
      encoder_input_port_(nullptr),
      encoder_output_port_(nullptr) {
    bcm_host_init();

    // Register our application with the logging system
    vcos_log_register("mmal_wrappper", VCOS_LOG_CATEGORY);

    // reset encoder setting to default state
    default_status(&state_);
    config_media_ = ConfigMediaSingleton::Instance();
}

MMALEncoderWrapper::~MMALEncoderWrapper() { RTC_LOG(INFO) << __FUNCTION__; }

int MMALEncoderWrapper::GetWidth() { return state_.width; }

int MMALEncoderWrapper::GetHeight() { return state_.height; }

bool MMALEncoderWrapper::IsInited() { return mmal_initialized_; }

void MMALEncoderWrapper::SetVideoRotation(int rotation) {
    state_.camera_parameters.rotation = rotation;
}

void MMALEncoderWrapper::SetVideoFlip(bool vflip, bool hflip) {
    state_.camera_parameters.vflip = (vflip ? 1 : 0);
    state_.camera_parameters.hflip = (hflip ? 1 : 0);
}

void MMALEncoderWrapper::SetVideoAnnotate(bool annotate_enable) {
    if (annotate_enable) {
        state_.camera_parameters.enable_annotate =
            (ANNOTATE_DATE_TEXT | ANNOTATE_TIME_TEXT |
             ANNOTATE_BLACK_BACKGROUND);
    } else {
        // disable annotation
        state_.camera_parameters.enable_annotate = 0;
        // clear previous setting value
        strcpy(state_.camera_parameters.annotate_string, "");
    }
}

void MMALEncoderWrapper::SetVideoAnnotateUserText(const std::string user_text) {
    if (user_text.length() > 0) {
        if (user_text.length() < MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2) {
            strcpy(state_.camera_parameters.annotate_string, user_text.c_str());
        } else {
            strncpy(state_.camera_parameters.annotate_string, user_text.c_str(),
                    MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2);
        }
        state_.camera_parameters.enable_annotate |= ANNOTATE_USER_TEXT;
    }
}

void MMALEncoderWrapper::SetVideoAnnotateTextSize(const int text_size) {
    state_.camera_parameters.annotate_text_size = text_size;
}

void MMALEncoderWrapper::SetVideoAnnotateTextSizeRatio(
    const int text_size_ratio) {
    state_.camera_parameters.annotate_text_size_ratio = text_size_ratio;
}

void MMALEncoderWrapper::SetInlineMotionVectors(bool motion_enable) {
    state_.inlineMotionVectors = motion_enable;
}

void MMALEncoderWrapper::SetIntraPeriod(int frame_period) {
    state_.intraperiod = frame_period;
}

void MMALEncoderWrapper::SetVideoSharpness(const int sharpness) {
    state_.camera_parameters.sharpness = sharpness;
}

void MMALEncoderWrapper::SetVideoContrast(const int contrast) {
    state_.camera_parameters.contrast = contrast;
}

void MMALEncoderWrapper::SetVideoBrightness(const int brightness) {
    state_.camera_parameters.brightness = brightness;
}

void MMALEncoderWrapper::SetVideoSaturation(const int saturation) {
    state_.camera_parameters.saturation = saturation;
}

void MMALEncoderWrapper::SetVideoEV(const int ev) {
    state_.camera_parameters.exposureCompensation = ev;
}

void MMALEncoderWrapper::SetVideoExposureMode(const std::string exposure_mode) {
    state_.camera_parameters.exposureMode =
        exposure_mode_from_string(exposure_mode.c_str());
}

void MMALEncoderWrapper::SetVideoFlickerMode(const std::string flicker_mode) {
    state_.camera_parameters.flickerAvoidMode =
        flicker_avoid_mode_from_string(flicker_mode.c_str());
}

void MMALEncoderWrapper::SetVideoAwbMode(const std::string awb_mode) {
    state_.camera_parameters.awbMode = awb_mode_from_string(awb_mode.c_str());
}

void MMALEncoderWrapper::SetVideoDrcMode(const std::string drc_mode) {
    state_.camera_parameters.drc_level = drc_mode_from_string(drc_mode.c_str());
}

void MMALEncoderWrapper::SetVideoVideoStabilisation(bool stab_enable) {
    state_.camera_parameters.videoStabilisation = stab_enable;
}

void MMALEncoderWrapper::SetMediaConfigParams() {
    // reset encoder setting to default state
    if (mmal_initialized_ == false) {
        //  state_ resetting to default is done only if the Encoder is not
        //  inited.
        default_status(&state_);
    };
    // Setting Camera Number
    // cameraNum sets the config value as it is.
    // There is no need to change or use this value internally.
    state_.cameraNum = config_media_->GetCameraSelect();

    // Setting Video Rotation and Flip setting
    SetVideoRotation(config_media_->GetVideoRotation());
    SetVideoFlip(config_media_->GetVideoVFlip(),
                 config_media_->GetVideoHFlip());

    // Video Image related parameter settings
    SetVideoSharpness(config_media_->GetVideoSharpness());
    SetVideoContrast(config_media_->GetVideoContrast());
    SetVideoBrightness(config_media_->GetVideoBrightness());
    SetVideoSaturation(config_media_->GetVideoSaturation());
    SetVideoEV(config_media_->GetVideoEV());
    SetVideoExposureMode(config_media_->GetVideoExposureMode());
    SetVideoFlickerMode(config_media_->GetVideoFlickerMode());
    SetVideoAwbMode(config_media_->GetVideoAwbMode());
    SetVideoDrcMode(config_media_->GetVideoDrcMode());
    SetVideoVideoStabilisation(config_media_->GetVideoStabilisation());

    // Video Annotation
    bool video_enable_annotate_text =
        config_media_->GetVideoEnableAnnotateText();
    SetVideoAnnotate(video_enable_annotate_text);
    if (video_enable_annotate_text == true) {
        SetVideoAnnotateUserText(config_media_->GetVideoAnnotateText());
        SetVideoAnnotateTextSizeRatio(
            config_media_->GetVideoAnnotateTextSizeRatio());
    };
}

bool MMALEncoderWrapper::InitEncoder(int width, int height, int framerate,
                                     int bitrate) {
    MMAL_STATUS_T status = MMAL_SUCCESS;

    RTC_LOG(INFO) << "Start initialize the MMAL encode wrapper." << width << "x"
                  << height << "@" << framerate << ", " << bitrate << "kbps";

    webrtc::MutexLock lock(&mutex_);
    if (mmal_initialized_ == true) return true;

    state_.width = width;
    state_.height = height;
    state_.framerate = framerate;
    state_.bitrate = bitrate * 1000;

    // set annotation text size based on text size ratio
    if (state_.camera_parameters.annotate_text_size_ratio != 0) {
        state_.camera_parameters.annotate_text_size =
            (width * state_.camera_parameters.annotate_text_size_ratio) / 100 +
            1;
    }

    if ((status = create_camera_component(&state_)) != MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "Failed to create camera component";
    } else if ((status = raspipreview_create(&state_.preview_parameters)) !=
               MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "Failed to create preview component";
        destroy_camera_component(&state_);
    } else if ((status = create_encoder_component(&state_)) != MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "Failed to create encode component";
        raspipreview_destroy(&state_.preview_parameters);
        destroy_camera_component(&state_);
    } else {
        if (state_.verbose)
            RTC_LOG(INFO) << "Starting component connection stage";

        camera_preview_port_ =
            state_.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
        camera_video_port_ =
            state_.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
        camera_still_port_ =
            state_.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
        preview_input_port_ =
            state_.preview_parameters.preview_component->input[0];
        encoder_input_port_ = state_.encoder_component->input[0];
        encoder_output_port_ = state_.encoder_component->output[0];

        if (state_.preview_parameters.wantPreview) {
            if (state_.verbose) {
                RTC_LOG(INFO)
                    << "Connecting camera preview port to preview input port";
                RTC_LOG(INFO) << "Starting video preview";
            }

            // Connect camera to preview
            status = connect_ports(camera_preview_port_, preview_input_port_,
                                   &state_.preview_connection);

            if (status != MMAL_SUCCESS) {
                state_.preview_connection = nullptr;
                return false;
            }
        }

        if (state_.verbose)
            RTC_LOG(INFO)
                << "Connecting camera stills port to encoder input port";

        // Now connect the camera to the encoder
        status = connect_ports(camera_video_port_, encoder_input_port_,
                               &state_.encoder_connection);
        if (status != MMAL_SUCCESS) {
            state_.encoder_connection = nullptr;
            RTC_LOG(LS_ERROR)
                << "Failed to connect camera video port to encoder input";
            return false;
        }

        // Set up our userdata - this is passed though to the callback
        // where we need the information.
        state_.callback_data.pstate = &state_;
        state_.callback_data.abort = 0;

        encoder_output_port_->userdata = (struct MMAL_PORT_USERDATA_T *)this;
        if (state_.verbose) RTC_LOG(INFO) << "Enabling encoder output port";

        // FrameQueue Init
        Init();

        // Enable the encoder output port and tell it its callback function
        status = mmal_port_enable(encoder_output_port_, BufferCallback);
        if (status != MMAL_SUCCESS) {
            RTC_LOG(LS_ERROR) << "Failed to setup encoder output";
            return false;
        }

        // dump_all_mmal_component(&state_);
        mmal_initialized_ = true;
        return true;
    }

    // Something failed, so need to check Camera Config
    // and log the problem of camera config
    CheckCameraConfig();
    return false;
}

bool MMALEncoderWrapper::ReinitEncoderInternal() {
    return ReinitEncoder(state_.width, state_.height, state_.framerate,
                         state_.bitrate / 1000);
}

bool MMALEncoderWrapper::ReinitEncoder(int width, int height, int framerate,
                                       int bitrate /* kbps */) {
    MMAL_STATUS_T status = MMAL_SUCCESS;

    if (mmal_initialized_ == false) {
        RTC_LOG(LS_ERROR) << "Trying to Reinitialize MMAL commpoent, but not "
                             "initialized before";
        return false;
    }

    state_.width = width;
    state_.height = height;
    state_.framerate = framerate;
    state_.bitrate = bitrate * 1000;

    // set annotation text size based on text size ratio
    if (state_.camera_parameters.annotate_text_size_ratio != 0) {
        state_.camera_parameters.annotate_text_size =
            (width * state_.camera_parameters.annotate_text_size_ratio) / 100 +
            1;
    }

    webrtc::MutexLock lock(&mutex_);

    RTC_LOG(INFO) << "Start reinitialize the MMAL encode wrapper." << width
                  << "x" << height << "@" << framerate << ", " << bitrate
                  << "kbps";

    //
    // disable all component
    //
    if (StopCapture() == false) {
        RTC_LOG(LS_ERROR) << "Unable to unset capture start";
        return false;
    }

    // Disable all our ports that are not handled by connections
    check_disable_port(camera_still_port_);
    check_disable_port(encoder_output_port_);
    check_disable_port(state_.camera_component->control);

    if (state_.preview_parameters.wantPreview && state_.preview_connection)
        mmal_connection_destroy(state_.preview_connection);

    if (state_.encoder_connection)
        mmal_connection_destroy(state_.encoder_connection);

    /* Disable components */
    if (state_.encoder_component)
        mmal_component_disable(state_.encoder_component);

    if (state_.preview_parameters.preview_component)
        mmal_component_disable(state_.preview_parameters.preview_component);

    if (state_.camera_component)
        mmal_component_disable(state_.camera_component);

    //
    // Reseting all component
    //
    if ((status = reset_camera_component(&state_)) != MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "Failed to create camera component";
        return false;
    } else if ((status = raspipreview_create(&state_.preview_parameters)) !=
               MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "Failed to create preview component";
        destroy_camera_component(&state_);
        return false;
    } else if ((status = reset_encoder_component(&state_)) != MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "Failed to create encode component";
        raspipreview_destroy(&state_.preview_parameters);
        destroy_camera_component(&state_);
        return false;
    } else {
        if (state_.verbose)
            RTC_LOG(INFO) << "Starting component connection stage";

        camera_preview_port_ =
            state_.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
        camera_video_port_ =
            state_.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
        camera_still_port_ =
            state_.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
        preview_input_port_ =
            state_.preview_parameters.preview_component->input[0];
        encoder_input_port_ = state_.encoder_component->input[0];
        encoder_output_port_ = state_.encoder_component->output[0];

        if (state_.preview_parameters.wantPreview) {
            if (state_.verbose) {
                RTC_LOG(INFO)
                    << "Connecting camera preview port to preview input port";
                RTC_LOG(INFO) << "Starting video preview";
            }

            // Connect camera to preview
            status = connect_ports(camera_preview_port_, preview_input_port_,
                                   &state_.preview_connection);

            if (status != MMAL_SUCCESS) {
                state_.preview_connection = nullptr;
                return false;
            }
        }

        if (state_.verbose)
            RTC_LOG(INFO)
                << "Connecting camera stills port to encoder input port";

        // Now connect the camera to the encoder
        status = connect_ports(camera_video_port_, encoder_input_port_,
                               &state_.encoder_connection);
        if (status != MMAL_SUCCESS) {
            state_.encoder_connection = nullptr;
            RTC_LOG(LS_ERROR)
                << "Failed to connect camera video port to encoder input";
            return false;
        }

        // Set up our userdata - this is passed though to the callback
        // where we need the information.
        state_.callback_data.pstate = &state_;
        state_.callback_data.abort = 0;

        encoder_output_port_->userdata = (struct MMAL_PORT_USERDATA_T *)this;
        if (state_.verbose) RTC_LOG(INFO) << "Enabling encoder output port";

        // FrameQueue Init
        Init();

        // Enable the encoder output port and tell it its callback function
        status = mmal_port_enable(encoder_output_port_, BufferCallback);
        if (status != MMAL_SUCCESS) {
            RTC_LOG(LS_ERROR) << "Failed to setup encoder output";
            return false;
        }

        // dump_all_mmal_component(&state_);
        return true;
    }

    RTC_LOG(LS_ERROR) << "Not Reached";
    return false;
}

bool MMALEncoderWrapper::IsDigitalZoomActive() {
    if (state_.camera_parameters.roi.w == 1.0 &&
        state_.camera_parameters.roi.h == 1.0)
        return true;  // using 1.0/1.0 width/height
    return false;
}

bool MMALEncoderWrapper::IncreaseDigitalZoom(double cx, double cy) {
    return raspicamcontrol_zoom_with_coordination(
        state_.camera_component, ZOOM_IN, cx, cy,
        &(state_.camera_parameters).roi);
}

bool MMALEncoderWrapper::DecreaseDigitalZoom() {
    return raspicamcontrol_zoom_with_coordination(
        state_.camera_component, ZOOM_OUT, 0.0, 0.0,
        &(state_.camera_parameters).roi);
}

bool MMALEncoderWrapper::ResetDigitalZoom() {
    return raspicamcontrol_zoom_with_coordination(
        state_.camera_component, ZOOM_RESET, 0.0, 0.0,
        &(state_.camera_parameters).roi);
}

bool MMALEncoderWrapper::MoveDigitalZoom(double cx, double cy) {
    return raspicamcontrol_zoom_with_coordination(
        state_.camera_component, ZOOM_MOVE, cx, cy,
        &(state_.camera_parameters).roi);
}

void MMALEncoderWrapper::BufferCallback(MMAL_PORT_T *port,
                                        MMAL_BUFFER_HEADER_T *buffer) {
    reinterpret_cast<MMALEncoderWrapper *>(port->userdata)
        ->OnBufferCallback(port, buffer);
}

bool MMALEncoderWrapper::UninitEncoder() {
    webrtc::MutexLock lock(&mutex_);
    if (mmal_initialized_ == false) return true;

    RTC_LOG(INFO) << "unitialize the MMAL encode wrapper.";

    // Disable all our ports that are not handled by connections
    check_disable_port(camera_still_port_);
    check_disable_port(encoder_output_port_);

    if (state_.preview_parameters.wantPreview && state_.preview_connection)
        mmal_connection_destroy(state_.preview_connection);

    if (state_.encoder_connection)
        mmal_connection_destroy(state_.encoder_connection);

    /* Disable components */
    if (state_.encoder_component)
        mmal_component_disable(state_.encoder_component);

    if (state_.preview_parameters.preview_component)
        mmal_component_disable(state_.preview_parameters.preview_component);

    if (state_.camera_component)
        mmal_component_disable(state_.camera_component);

    destroy_encoder_component(&state_);
    raspipreview_destroy(&state_.preview_parameters);
    destroy_camera_component(&state_);
    mmal_initialized_ = false;
    return true;
}

void MMALEncoderWrapper::OnBufferCallback(MMAL_PORT_T *port,
                                          MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_BUFFER_HEADER_T *new_buffer = nullptr;
    static int64_t base_time = -1;
    static int64_t last_second = -1;
    int64_t current_time = vcos_getmicrosecs64() / 1000;

    // All our segment times based on the receipt of the first encoder callback
    if (base_time == -1) base_time = vcos_getmicrosecs64() / 1000;

    // We pass our file handle and other stuff in via the userdata field.
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    if (pData) {
        HandlingMMALFrame(buffer);
    } else {
        vcos_log_error("Received a encoder buffer callback with no state");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled) {
        MMAL_STATUS_T status;

        new_buffer = mmal_queue_get(state_.encoder_pool->queue);

        if (new_buffer) status = mmal_port_send_buffer(port, new_buffer);

        if (!new_buffer || status != MMAL_SUCCESS)
            RTC_LOG(LS_ERROR)
                << "Unable to return a buffer to the encoder port";
    }

    // See if the second count has changed and we need to update any annotation
    if (current_time / 1000 != last_second) {
        update_annotation_data(&state_);
        last_second = current_time / 1000;
    }
}

bool MMALEncoderWrapper::StartCapture() {
    // Send all the buffers to the encoder output port
    int num = mmal_queue_length(state_.encoder_pool->queue);

    for (int q = 0; q < num; q++) {
        MMAL_BUFFER_HEADER_T *buffer =
            mmal_queue_get(state_.encoder_pool->queue);

        if (!buffer)
            RTC_LOG(LS_ERROR) << "Unable to get a required buffer " << q
                              << " from pool queue";

        if (mmal_port_send_buffer(encoder_output_port_, buffer) != MMAL_SUCCESS)
            RTC_LOG(LS_ERROR)
                << "Unable to send a buffer to encoder output port (" << q
                << ").";
    }

    if (mmal_port_parameter_set_boolean(camera_video_port_,
                                        MMAL_PARAMETER_CAPTURE,
                                        MMAL_TRUE) != MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "Unable to set capture start";
        return false;
    }
    RTC_LOG(INFO) << "capture started.";

    return true;
}

bool MMALEncoderWrapper::StopCapture() {
    if (mmal_port_parameter_set_boolean(camera_video_port_,
                                        MMAL_PARAMETER_CAPTURE,
                                        MMAL_FALSE) != MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "Unable to unset capture start";
        return false;
    }
    RTC_LOG(INFO) << "capture stopped.";
    return true;
}

//
bool MMALEncoderWrapper::SetEncodingParams(int width, int height, int framerate,
                                           int bitrate) {
    if (state_.width != width || state_.height != height ||
        state_.framerate != framerate || state_.bitrate != bitrate) {
        webrtc::MutexLock lock(&mutex_);

        state_.width = width;
        state_.height = height;
        state_.framerate = framerate;
        state_.bitrate = bitrate;
        return true;
    }
    return false;
}

//
bool MMALEncoderWrapper::SetRate(int framerate, int bitrate) {
    if (mmal_initialized_ == false) return true;
    if (state_.framerate != framerate || state_.bitrate != bitrate * 1000) {
        webrtc::MutexLock lock(&mutex_);
        MMAL_STATUS_T status;

        if (state_.bitrate != bitrate * 1000) {
            MMAL_PARAMETER_UINT32_T param = {
                {MMAL_PARAMETER_VIDEO_BIT_RATE, sizeof(param)},
                (uint32_t)bitrate * 1000};

            // {{ MMAL_PARAMETER_VIDEO_ENCODE_PEAK_RATE, sizeof(param)},
            // (uint32_t)bitrate*1000};
            status = mmal_port_parameter_set(encoder_output_port_, &param.hdr);
            if (status != MMAL_SUCCESS) {
                RTC_LOG(LS_ERROR) << "Unable to set bitrate";
            }
        }

        if (state_.framerate != framerate) {  // frame rate
            RTC_LOG(INFO) << "framerate changed : " << framerate << " fps";
            MMAL_PARAMETER_FRAME_RATE_T param;
            param.hdr.id = MMAL_PARAMETER_FRAME_RATE;
            param.hdr.size = sizeof(param);
            param.frame_rate.num = framerate;
            param.frame_rate.den = 1;

            // status = mmal_port_parameter_set(encoder_output_port_,
            // &param.hdr);
            status = mmal_port_parameter_set(camera_video_port_, &param.hdr);
            if (status != MMAL_SUCCESS) {
                RTC_LOG(LS_ERROR) << "Unable to set framerate";
            }
        }
        state_.bitrate = bitrate * 1000;
        state_.framerate = framerate;
    }
    return true;
}

bool MMALEncoderWrapper::SetForceNextKeyFrame() {
    if (mmal_initialized_ == false) return true;
    webrtc::MutexLock lock(&mutex_);
    RTC_LOG(INFO) << "MMAL force key frame encoding";

    if (mmal_port_parameter_set_boolean(encoder_output_port_,
                                        MMAL_PARAMETER_VIDEO_REQUEST_I_FRAME,
                                        MMAL_TRUE) != MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "failed to request KeyFrame";
        return false;
    }
    return true;
}

// bollowed from raspicamcontrol_check_configuration in raspicomcontrol.c
// for camera config error message logging
//
// This function should be called when InitEncoder had failed
void MMALEncoderWrapper::CheckCameraConfig() {
    int min_gpu_mem = 128;  // minimum memory is 128M
    int gpu_mem = raspicamcontrol_get_mem_gpu();
    int supported = 0, detected = 0;
    raspicamcontrol_get_camera(&supported, &detected);
    if (!supported) {
        RTC_LOG(LS_ERROR) << "Camera is not enabled in this build. "
                          << "Try running \"sudo raspi-config\" and ensure "
                          << "that \"camera\" has been enabled";
    } else if (gpu_mem < min_gpu_mem) {
        RTC_LOG(LS_ERROR) << "Only " << gpu_mem
                          << "M of gpu_mem is configured. Try running "
                          << "\"sudo raspi-config\" and ensure that "
                             "\"memory_split\" has a value of "
                          << min_gpu_mem << " or greater";
    } else if (!detected) {
        RTC_LOG(LS_ERROR) << "Camera is not detected. Please check carefully "
                          << "the camera module is installed correctly";
    } else {
        RTC_LOG(LS_ERROR)
            << "Failed to run camera app. Please check for firmware updates";
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// MMALWrapper Sigletone interface
//
////////////////////////////////////////////////////////////////////////////////////////

MMALEncoderWrapper *MMALWrapper::mmal_wrapper_ = nullptr;
std::once_flag MMALWrapper::singleton_flag_;

void MMALWrapper::createMMALWrapperSingleton() {
    mmal_wrapper_ = new MMALEncoderWrapper();
}

MMALEncoderWrapper *MMALWrapper::Instance() {
    std::call_once(singleton_flag_, createMMALWrapperSingleton);
    return mmal_wrapper_;
}

MMALWrapper::~MMALWrapper() {
    // By above RTC_DEFINE_STATIC_LOCAL.
    RTC_NOTREACHED() << "MMALWrapper should never be destructed.";
}

MMALWrapper::MMALWrapper() { RTC_NOTREACHED(); }

}  // namespace webrtc
