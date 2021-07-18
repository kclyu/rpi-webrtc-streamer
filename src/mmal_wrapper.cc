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

////////////////////////////////////////////////////////////////////////////////
//
// MMAL Encoder Delayed Reinit
//
////////////////////////////////////////////////////////////////////////////////
namespace {

constexpr int kDelayInitialDurationMs = 4000;
constexpr int kDelayTaskActiveInterval = 500;
constexpr int kDelayTaskIdleInterval = 1000;
constexpr int kInitCoolingDownPeriodMs = 2000;
constexpr int kInitDelayingPeriodMs = 2000;

}  // namespace

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
        : encoder_delay_init_(encoder_delay_init_) {}

   private:
    bool Run() override {
        if (encoder_delay_init_->IsEncodingActive() == false) {
            return true;  // TaskQueue will free this task.
        }

        encoder_delay_init_->UpdateStateOrMayInit();
        webrtc::TaskQueueBase::Current()->PostDelayedTask(
            std::unique_ptr<webrtc::QueuedTask>(this),
            encoder_delay_init_->IsIdleState() ? kDelayTaskIdleInterval
                                               : kDelayTaskActiveInterval);
        return false;  // Retain the task in order to reuse it.
    }

    EncoderDelayedInit *const encoder_delay_init_;
};

EncoderDelayedInit::EncoderDelayedInit(MMALEncoderWrapper *mmal_encoder)
    : task_queue_factory_(webrtc::CreateDefaultTaskQueueFactory()),
      task_queue_(task_queue_factory_->CreateTaskQueue(
          "DelayedInit", webrtc::TaskQueueFactory::Priority::HIGH)),
      clock_(Clock::GetRealTimeClock()),
      last_init_timestamp_ms_(clock_->TimeInMilliseconds()),
      state_(IDLE),
      mmal_encoder_(mmal_encoder) {}

EncoderDelayedInit::~EncoderDelayedInit() {}

bool EncoderDelayedInit::IsEncodingActive() {
    return mmal_encoder_ && mmal_encoder_->IsInited();
}

bool EncoderDelayedInit::InitEncoder(wstreamer::VideoEncodingParams config) {
    if (mmal_encoder_->IsInited()) {
        RTC_LOG(LS_ERROR) << "MMAL Encoder already initialized.";
        return true;
    };

    RTC_LOG(INFO) << "InitEncoder " << config.ToString();
    RTC_LOG(LS_INFO)
        << "Created EncoderDelayedInit Task, Scheduling on queue...";
    task_queue_.PostDelayedTask(
        std::make_unique<EncoderDelayedInit::DelayInitTask>(this),
        kDelayInitialDurationMs);

    // InitEncoder does not need to do any init delay
    RTC_LOG(INFO) << "EncoderDelay state changed from IDLE to COOLINGDOWN";
    state_ = INIT_COOLINGDOWN;
    return mmal_encoder_->InitEncoder(config);
}

bool EncoderDelayedInit::ReinitEncoder(wstreamer::VideoEncodingParams config) {
    if (mmal_encoder_->IsInited() == false) {
        RTC_LOG(LS_ERROR) << "MMAL Encoder does not initialized.";
        return false;
    };
    RTC_LOG(INFO) << "ReinitEncoder " << config.ToString();

    if (state_ == IDLE) {
        state_ = INIT_COOLINGDOWN;
        RTC_LOG(INFO) << "EncoderDelay state changed from IDLE to COOLINGDOWN";
        if (mmal_encoder_->ReinitEncoder(config) == false) {
            RTC_LOG(LS_ERROR) << "Failed to reinitialize MMAL encoder";
            return false;
        };
        // start capture in here
        mmal_encoder_->StartCapture();
        return true;
    } else if (state_ == DELAYING_INIT) {
        mmal_encoder_->SetEncodingParams(config);
    } else if (state_ == INIT_COOLINGDOWN) {
        if (mmal_encoder_->SetEncodingParams(config)) {
            state_ = DELAYING_INIT;
            RTC_LOG(INFO)
                << "EncoderDelay state changed from COOLINGDOWN to DELAY";
        }
    }

    return true;
}

bool EncoderDelayedInit::UpdateStateOrMayInit() {
    uint64_t timestamp_diff =
        clock_->TimeInMilliseconds() - last_init_timestamp_ms_;

    if (state_ == DELAYING_INIT) {
        if (timestamp_diff > kInitDelayingPeriodMs) {
            state_ = INIT_COOLINGDOWN;
            RTC_LOG(INFO)
                << "EncoderDelay state changed from DELAYING to COOLINGDOWN";
            if (mmal_encoder_->ReinitEncoderInternal() == false) {
                RTC_LOG(LS_ERROR) << "Failed to reinitialize MMAL encoder";
                return false;
            }
            // start capture in here
            mmal_encoder_->StartCapture();
        };
    } else if (state_ == INIT_COOLINGDOWN) {
        if (timestamp_diff > kInitCoolingDownPeriodMs) {
            RTC_LOG(INFO)
                << "EncoderDelay state changed from COOLINGDOWN to IDLE";
            state_ = IDLE;
        };
    }
    last_init_timestamp_ms_ = clock_->TimeInMilliseconds();
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// MMAL Encoder Wrapper
//
////////////////////////////////////////////////////////////////////////////////

MMALEncoderWrapper::MMALEncoderWrapper()
    : encoder_delayed_init_(this),
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

void MMALEncoderWrapper::SetEncoderConfigParams(
    wstreamer::EncoderSettings *params) {
    // reset encoder setting to default state
    if (mmal_initialized_ == false) {
        //  state_ resetting to default is done only if the Encoder is not
        default_status(&state_);
    };
    // Setting Camera Number
    // cameraNum sets the config value as it is.
    // There is no need to change or use this value internally.
    state_.cameraNum = config_media_->GetCameraSelect();

    // Setting Video ROI
    {
        wstreamer::VideoRoi roi = config_media_->GetVideoROI();
        state_.camera_parameters.roi.x = roi.x_;
        state_.camera_parameters.roi.y = roi.y_;
        state_.camera_parameters.roi.h = roi.height_;
        state_.camera_parameters.roi.w = roi.width_;
    }

    // Setting Video Rotation and Flip setting
    state_.camera_parameters.rotation = config_media_->GetVideoRotation();

    state_.camera_parameters.vflip = (config_media_->GetVideoVFlip() ? 1 : 0);
    state_.camera_parameters.hflip = (config_media_->GetVideoHFlip() ? 1 : 0);

    state_.camera_parameters.sharpness = config_media_->GetVideoSharpness();
    state_.camera_parameters.contrast = config_media_->GetVideoContrast();
    state_.camera_parameters.brightness = config_media_->GetVideoBrightness();
    state_.camera_parameters.saturation = config_media_->GetVideoSaturation();
    state_.camera_parameters.exposureCompensation = config_media_->GetVideoEV();

    state_.camera_parameters.exposureMode = exposure_mode_from_string(
        config_media_->GetVideoExposureMode().c_str());
    state_.camera_parameters.flickerAvoidMode = flicker_avoid_mode_from_string(
        config_media_->GetVideoFlickerMode().c_str());

    state_.camera_parameters.awbMode =
        awb_mode_from_string(config_media_->GetVideoAwbMode().c_str());

    state_.camera_parameters.drc_level =
        drc_mode_from_string(config_media_->GetVideoDrcMode().c_str());

    state_.camera_parameters.videoStabilisation =
        config_media_->GetVideoStabilisation();

    // annotation config from ConfigMedia
    if (config_media_->GetVideoEnableAnnotateText()) {
        state_.camera_parameters.enable_annotate =
            (ANNOTATE_DATE_TEXT | ANNOTATE_TIME_TEXT |
             ANNOTATE_BLACK_BACKGROUND);

        const std::string annotation_user_text =
            config_media_->GetVideoAnnotateText();
        if (annotation_user_text.length() > 0) {
            if (annotation_user_text.length() <
                MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2) {
                strcpy(state_.camera_parameters.annotate_string,
                       annotation_user_text.c_str());
            } else {
                strncpy(state_.camera_parameters.annotate_string,
                        annotation_user_text.c_str(),
                        MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2);
            }
            state_.camera_parameters.annotate_text_size_ratio =
                config_media_->GetVideoAnnotateTextSizeRatio();
        } else {
            state_.camera_parameters.enable_annotate |= ANNOTATE_USER_TEXT;
        }

    } else {
        // disable annotation
        state_.camera_parameters.enable_annotate = 0;
        // clear previous setting value
        strcpy(state_.camera_parameters.annotate_string, "");
    }

    // setting additional params
    if (params) {
        // default boolean value of Inline Motion Vector is false
        state_.inlineMotionVectors = params->imv_enable.value_or(false);
        // default the value of intraperiod is 3 seconds
        state_.intraperiod = params->intra_period.value_or(
            state_.framerate * VIDEO_INTRAFRAME_PERIOD);
        if (params->annotation_enable.value_or(false)) {
            // custom Annotation settings
            state_.camera_parameters.enable_annotate = true;
            const std::string annotation_user_text =
                params->annotation_text.value_or("");
            if (annotation_user_text.length() > 0) {
                if (annotation_user_text.length() <
                    MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2) {
                    strcpy(state_.camera_parameters.annotate_string,
                           annotation_user_text.c_str());
                } else {
                    strncpy(state_.camera_parameters.annotate_string,
                            annotation_user_text.c_str(),
                            MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2);
                }
            } else {
                state_.camera_parameters.enable_annotate |= ANNOTATE_USER_TEXT;
            }
            state_.camera_parameters.annotate_text_size =
                params->annotation_text_size.value_or(32);
        }
        // annotation config from Params arguments
    } else {
        state_.inlineMotionVectors = false;  // default is false
        state_.intraperiod = state_.framerate * VIDEO_INTRAFRAME_PERIOD;
        state_.camera_parameters.enable_annotate = false;
    }
}

bool MMALEncoderWrapper::InitEncoder(wstreamer::VideoEncodingParams config) {
    MMAL_STATUS_T status = MMAL_SUCCESS;

    RTC_LOG(INFO) << "Start initialize the MMAL encode wrapper."
                  << config.ToString();

    webrtc::MutexLock lock(&mutex_);
    if (mmal_initialized_ == true) return true;

    state_.width = config.width_;
    state_.height = config.height_;
    state_.framerate = config.framerate_;
    state_.bitrate = config.bitrate_ * 1000;

    // set annotation text size ratio based on video resolution
    if (state_.camera_parameters.annotate_text_size_ratio != 0) {
        state_.camera_parameters.annotate_text_size =
            (config.width_ *
             state_.camera_parameters.annotate_text_size_ratio) /
                100 +
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
                << "Connecting camera video port to encoder input port";

        // Now connect the camera to the encoder
        status = connect_ports(camera_video_port_, encoder_input_port_,
                               &state_.encoder_connection);
        if (status != MMAL_SUCCESS) {
            state_.encoder_connection = nullptr;
            RTC_LOG(LS_ERROR)
                << "Failed to connect camera video port to encoder input";
            return false;
        }

        encoder_output_port_->userdata = (struct MMAL_PORT_USERDATA_T *)this;
        if (state_.verbose) RTC_LOG(INFO) << "Enabling encoder output port";

        // Init Frame Queue with recommended frame buffer size
        recommanded_buffer_size_ =
            GetRecommandedBufferSize(encoder_output_port_);
        recommanded_buffer_num_ = GetRecommandedBufferNum(encoder_output_port_);
        RTC_LOG(INFO) << "Queue Buffer size: " << recommanded_buffer_size_
                      << ", Buffer num: " << recommanded_buffer_num_;
        Init(recommanded_buffer_num_, recommanded_buffer_size_);

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
    return ReinitEncoder(wstreamer::VideoEncodingParams(
        state_.width, state_.height, state_.framerate, state_.bitrate / 1000));
}

bool MMALEncoderWrapper::ReinitEncoder(wstreamer::VideoEncodingParams config) {
    MMAL_STATUS_T status = MMAL_SUCCESS;

    if (mmal_initialized_ == false) {
        RTC_LOG(LS_ERROR) << "Trying to Reinitialize MMAL commpoent, but not "
                             "initialized before";
        return false;
    }

    state_.width = config.width_;
    state_.height = config.height_;
    state_.framerate = config.framerate_;
    state_.bitrate = config.bitrate_ * 1000;

    // The text size ratio is recalculated to make the test size look similar
    // to the changed video resolution.
    if (state_.camera_parameters.annotate_text_size_ratio != 0) {
        state_.camera_parameters.annotate_text_size =
            (config.width_ *
             state_.camera_parameters.annotate_text_size_ratio) /
                100 +
            1;
    }

    webrtc::MutexLock lock(&mutex_);

    RTC_LOG(INFO) << "Start reinitialize the MMAL encode wrapper."
                  << config.ToString();

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

        recommanded_buffer_size_ =
            GetRecommandedBufferSize(encoder_output_port_);
        recommanded_buffer_num_ = GetRecommandedBufferNum(encoder_output_port_);
        // Init Frame Queue with recommended frame buffer size
        Init(recommanded_buffer_num_, recommanded_buffer_size_);

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

bool MMALEncoderWrapper::Zoom(wstreamer::ZoomOptions options) {
    RTC_DCHECK(options.cmd >= wstreamer::ZoomOptions::IS_ACTIVE &&
               options.cmd <= wstreamer::ZoomOptions::RESET);
    using ZoomCommand = wstreamer::ZoomOptions::CMD;
    switch (options.cmd) {
        case ZoomCommand::IS_ACTIVE:
            // TODO: need to decide whether
            return !(state_.camera_parameters.roi.w == MAX_VIDEO_ROI_WIDTH &&
                     state_.camera_parameters.roi.h == MAX_VIDEO_ROI_HEIGHT);
        case ZoomCommand::IN:
            RTC_DCHECK(options.center_x.has_value());
            RTC_DCHECK(options.center_y.has_value());
            return raspicamcontrol_zoom_with_coordination(
                state_.camera_component, ZOOM_IN, options.center_x.value(),
                options.center_y.value(), &(state_.camera_parameters).roi);
        case ZoomCommand::OUT:
            return raspicamcontrol_zoom_with_coordination(
                state_.camera_component, ZOOM_OUT, 0.0, 0.0,
                &(state_.camera_parameters).roi);
        case ZoomCommand::MOVE:
            RTC_DCHECK(options.center_x.has_value());
            RTC_DCHECK(options.center_y.has_value());
            return raspicamcontrol_zoom_with_coordination(
                state_.camera_component, ZOOM_MOVE, options.center_x.value(),
                options.center_y.value(), &(state_.camera_parameters).roi);
        case ZoomCommand::RESET:
            return raspicamcontrol_zoom_with_coordination(
                state_.camera_component, ZOOM_RESET, 0.0, 0.0,
                &(state_.camera_parameters).roi);
    }
}

void MMALEncoderWrapper::BufferCallback(MMAL_PORT_T *port,
                                        MMAL_BUFFER_HEADER_T *buffer) {
    reinterpret_cast<MMALEncoderWrapper *>(port->userdata)
        ->OnBufferCallback(port, buffer);
}

size_t MMALEncoderWrapper::GetRecommandedBufferSize(MMAL_PORT_T *port) {
    RTC_LOG(INFO) << "MMAL Port Recommanded buffer size: "
                  << port->buffer_size_recommended;
    // normally, it required  the doube of port recommanded size
    // for safety,,
    return port->buffer_size_recommended * 3;
}

size_t MMALEncoderWrapper::GetRecommandedBufferNum(MMAL_PORT_T *port) {
    RTC_LOG(INFO) << "MMAL Port Recommanded buffer size: "
                  << port->buffer_num_recommended;
    // normally, it required  the doube of port recommanded size
    // for safety,,
    if (state_.inlineMotionVectors)
        return port->buffer_num_recommended * 3 * 2;
    else
        return port->buffer_num_recommended * 3;
}

bool MMALEncoderWrapper::UninitEncoder() {
    webrtc::MutexLock lock(&mutex_);
    if (mmal_initialized_ == false) return true;

    RTC_LOG(INFO) << "uninitialize the MMAL encode wrapper.";

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
    static int64_t last_second = -1;
    int64_t current_time = vcos_getmicrosecs64() / 1000;

    // We pass our file handle and other stuff in via the userdata field.
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    mmal_buffer_header_mem_lock(buffer);
    if (pData) {
        WriteBack(buffer);
    } else {
        vcos_log_error("Received a encoder buffer callback with no state");
    }
    mmal_buffer_header_mem_unlock(buffer);

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled) {
        MMAL_BUFFER_HEADER_T *new_buffer = nullptr;
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
bool MMALEncoderWrapper::SetEncodingParams(
    wstreamer::VideoEncodingParams config) {
    if (state_.width != config.width_ || state_.height != config.height_ ||
        state_.framerate != config.framerate_ ||
        state_.bitrate != config.bitrate_ * 1000) {
        state_.width = config.width_;
        state_.height = config.height_;
        state_.framerate = config.framerate_;
        state_.bitrate = config.bitrate_ * 1000;
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

bool MMALEncoderWrapper::RequestKeyFrame() {
    if (mmal_initialized_ == false) return true;
    webrtc::MutexLock lock(&mutex_);
    RTC_LOG(INFO) << "Send a request to generate a KeyFrame";

    if (mmal_port_parameter_set_boolean(encoder_output_port_,
                                        MMAL_PARAMETER_VIDEO_REQUEST_I_FRAME,
                                        MMAL_TRUE) != MMAL_SUCCESS) {
        RTC_LOG(LS_ERROR) << "failed to request KeyFrame";
        return false;
    }
    return true;
}

// borrowed from raspicamcontrol_check_configuration in raspicomcontrol.c
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

////////////////////////////////////////////////////////////////////////////////
//
// MMALWrapper Sigletone interface
//
////////////////////////////////////////////////////////////////////////////////

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
