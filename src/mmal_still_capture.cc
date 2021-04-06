/*
Copyright (c) 2021, rpi-webrtc-streamer Lyu,KeunChang

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

#include "mmal_still_capture.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <ctime>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "config_media.h"
#include "mmal_video.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/task_queue.h"
#include "utils.h"

namespace webrtc {

namespace {

constexpr int kStillDefaultTimeout = 5000;
constexpr bool kStillDefaultVerbose = false;

constexpr char kStillExtensionJpg[] = ".jpg";
constexpr char kStillExtensionPng[] = ".png";
constexpr char kStillExtensionGif[] = ".gif";
constexpr char kStillExtensionBmp[] = ".bmp";

struct FileInfo {
    std::string filename_;
    time_t age_;

    FileInfo(std::string filename, time_t age)
        : filename_(filename), age_(age) {}
};

static bool FileAgeCompare(const FileInfo &a, const FileInfo &b) {
    return a.age_ < b.age_;
}

bool GetLatestAndRemoveExpired(const std::string base_path,
                               std::string &latest_filename, int max_age) {
    std::list<FileInfo> file_list;
    std::string file_folder;
    // check video path is folder
    if (!utils::GetFolderWithTailingDelimiter(base_path, file_folder)) {
        return false;
    };

    std::time_t current_time = std::time(nullptr);
    DIR *dirp = ::opendir(file_folder.c_str());
    if (dirp == nullptr) return false;
    for (struct dirent *dirent = ::readdir(dirp); dirent;
         dirent = ::readdir(dirp)) {
        std::string name = dirent->d_name;
        // find still image files which is supported from the specified path
        if (absl::EndsWith(name, kStillExtensionJpg) ||
            absl::EndsWith(name, kStillExtensionBmp) ||
            absl::EndsWith(name, kStillExtensionPng) ||
            absl::EndsWith(name, kStillExtensionGif)) {
            time_t file_changed_time =
                utils::GetFileChangedTime(file_folder + name).value_or(0);
            file_list.emplace_back(name, current_time - file_changed_time);
        }
    }
    // not needed it anymore
    ::closedir(dirp);
    if (file_list.size() == 0) {
        RTC_LOG(INFO) << "no latest image still file in directory: "
                      << base_path;
        return false;
    }
    file_list.sort(FileAgeCompare);
    FileInfo latest_file_info = file_list.front();
    file_list.pop_front();

    for (FileInfo file_info : file_list) {
        if (file_info.age_ >= max_age) {
            RTC_LOG(INFO) << "file expired : "
                          << file_folder + file_info.filename_
                          << ", diff : " << file_info.age_;
            utils::DeleteFile(file_folder + file_info.filename_);
        }
    }
    RTC_LOG(INFO) << "latest still file: "
                  << file_folder + latest_file_info.filename_
                  << ", diff : " << latest_file_info.age_;
    // return false when file age is greater then max age
    if (latest_file_info.age_ >= max_age) return false;
    latest_filename = latest_file_info.filename_;
    return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// Stilll Capture
//
////////////////////////////////////////////////////////////////////////////////

StillCapture::StillCapture()
    : Event(false, false),
      still_capture_active_(false),
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
    default_still_status(&state_);
    config_media_ = ConfigMediaSingleton::Instance();
}

StillCapture::~StillCapture() { RTC_LOG(INFO) << __FUNCTION__; }

absl::Status StillCapture::GetLatestOrCapture(
    const wstreamer::StillOptions &options, std::string *captured_filename) {
    std::string latest_filename;
    if (options.force_capture.value_or(false) == true) {
        // do not need to get latest filename, so skipping the gettting latest
        // filename logic
        return Capture(options, captured_filename);
    };
    if (GetLatestAndRemoveExpired(config_media_->GetStillDirectory(),
                                  latest_filename,
                                  config_media_->GetStillMaxAge()) == true) {
        *captured_filename = latest_filename;
        return absl::OkStatus();
    }

    return Capture(options, captured_filename);
}

absl::Status StillCapture::Capture(const wstreamer::StillOptions &options,
                                   std::string *captured_filename) {
    absl::Status status;
    if ((status = InitStillEncoder(options)).ok() == false) {
        RTC_LOG(LS_ERROR) << "Failed to initialize still encoder : "
                          << status.ToString();
        return status;
    }
    //
    // StillDirectory + "/" + file_prefix [ + datatime ] + "." + extension
    //
    // file extension will be change to default when file extension is not
    // supported
    //
    const std::string filename = absl::StrCat(
        options.filename.value_or(config_media_->GetStillFilePrefix()),
        config_media_->GetStilFileAppendDataTime()
            ? "." + utils::GetDateTimeString()
            : "",
        ".", getExtensionByEncoding(state_.encoding));

    int open_error = 0;
    if ((file_ = FileWrapper::OpenWriteOnly(
             absl::StrCat(config_media_->GetStillDirectory(), "/", filename),
             &open_error))
            .is_open() == false) {
        UninitStillEncoder();
        return absl::InvalidArgumentError(absl::StrCat(
            "Failed to create file, error: ", strerror(open_error)));
    };

    if (state_.verbose) RTC_LOG(INFO) << "Starting still capturing";
    if (mmal_port_parameter_set_boolean(camera_still_port_,
                                        MMAL_PARAMETER_CAPTURE,
                                        MMAL_TRUE) != MMAL_SUCCESS) {
        return absl::InternalError("failed to start still capture");
    }

    // Waiting for capturing procedure finished
    if (Wait(state_.timeout)) {
        if (state_.verbose) RTC_LOG(INFO) << "Finished capture : " << filename;
        status = absl::OkStatus();
        if (captured_filename) *captured_filename = filename;
    } else {
        RTC_LOG(LS_ERROR) << "Failed to capture : " << filename << ", timeout";
        status = absl::AbortedError("timeout");
    }

    UninitStillEncoder();
    file_.Close();

    // successful to initialize still encoder
    return status;
}

void StillCapture::OnBufferCallback(MMAL_PORT_T *port,
                                    MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_BUFFER_HEADER_T *new_buffer = nullptr;
    if (buffer && buffer->length > 0) {
        file_.Write(buffer->data, buffer->length);
    }
    if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) {
        RTC_LOG(INFO) << "Frame buffer indicated capture ended";
        Set();
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
}

void StillCapture::BufferCallback(MMAL_PORT_T *port,
                                  MMAL_BUFFER_HEADER_T *buffer) {
    reinterpret_cast<StillCapture *>(port->userdata)
        ->OnBufferCallback(port, buffer);
}

absl::Status StillCapture::InitStillEncoder(
    const wstreamer::StillOptions &options) {
    InitParams(options);

    MMAL_STATUS_T status = MMAL_SUCCESS;
    if (state_.verbose) {
        dump_still_status(&state_);
    }

    if ((status = create_camera_still_component(&state_)) != MMAL_SUCCESS) {
        return absl::InternalError(
            absl::StrCat("Failed to create camera still component: ",
                         mmal_status_to_string(status)));
    } else if ((status = raspipreview_create(&state_.preview_parameters)) !=
               MMAL_SUCCESS) {
        destroy_camera_still_component(&state_);
        return absl::InternalError(
            absl::StrCat("Failed to create preview component: ",
                         mmal_status_to_string(status)));
    } else if ((status = create_encoder_still_component(&state_)) !=
               MMAL_SUCCESS) {
        raspipreview_destroy(&state_.preview_parameters);
        destroy_camera_still_component(&state_);
        return absl::InternalError(
            absl::StrCat("Failed to create encode component: ",
                         mmal_status_to_string(status)));
    }

    if (state_.verbose) RTC_LOG(INFO) << "Starting component connection stage";

    camera_preview_port_ =
        state_.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
    camera_video_port_ =
        state_.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
    camera_still_port_ =
        state_.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
    encoder_input_port_ = state_.encoder_component->input[0];
    encoder_output_port_ = state_.encoder_component->output[0];

    if (state_.verbose)
        RTC_LOG(INFO) << "Connecting camera preview port to video render.";

    // Note we are lucky that the preview and null sink components use the
    // same input port so we can simple do this without conditionals
    preview_input_port_ = state_.preview_parameters.preview_component->input[0];

    // Connect camera to preview (which might be a null_sink if no preview
    // required)
    status = connect_ports(camera_preview_port_, preview_input_port_,
                           &state_.preview_connection);
    if (status != MMAL_SUCCESS) {
        UninitStillEncoder();
        return absl::InternalError(
            absl::StrCat("Failed to connect camera to preview: ",
                         mmal_status_to_string(status)));
    }

    if (state_.verbose)
        RTC_LOG(INFO) << "Connecting camera stills port to encoder input port";

    // Now connect the camera to the encoder
    status = connect_ports(camera_still_port_, encoder_input_port_,
                           &state_.encoder_connection);
    if (status != MMAL_SUCCESS) {
        UninitStillEncoder();
        return absl::InternalError(absl::StrCat(
            "Failed to connect camera video port to encoder input: ",
            mmal_status_to_string(status)));
    }

    encoder_output_port_->userdata = (struct MMAL_PORT_USERDATA_T *)this;
    if (state_.verbose) RTC_LOG(INFO) << "Enabling encoder output port";

    // Enable the encoder output port and tell it its
    // callback function
    status = mmal_port_enable(encoder_output_port_, BufferCallback);
    if (status != MMAL_SUCCESS) {
        UninitStillEncoder();
        return absl::InternalError(absl::StrCat(
            "Failed to enable encoder port: ", mmal_status_to_string(status)));
    }

    // Send all the buffers to the encoder output port
    int num = mmal_queue_length(state_.encoder_pool->queue);
    for (int q = 0; q < num; q++) {
        MMAL_BUFFER_HEADER_T *buffer =
            mmal_queue_get(state_.encoder_pool->queue);

        if (!buffer)
            RTC_LOG(LS_ERROR) << "Unable to get a required buffer " << q
                              << " from pool queue";

        if (mmal_port_send_buffer(encoder_output_port_, buffer) != MMAL_SUCCESS)
            RTC_LOG(LS_ERROR) << "Unable to send a buffer to encoder output "
                                 "port "
                              << q;
    }

    // successful to initialize still encoder
    return absl::OkStatus();
}

void StillCapture::InitParams(const wstreamer::StillOptions &options) {
    default_still_status(&state_);
    state_.width = options.width.value_or(config_media_->GetStillWidth());
    state_.height = options.height.value_or(config_media_->GetStillHeight());
    state_.timeout = options.timeout.value_or(kStillDefaultTimeout);
    state_.verbose = options.verbose.value_or(kStillDefaultVerbose);
    state_.quality = options.quality.value_or(config_media_->GetStillQuality());
    state_.encoding = getEncodingByExtension(
        options.extension.value_or(config_media_->GetStillFileExtension())
            .c_str());

    // Still Camera Num has more higher proority then Video CameraSelect of
    // video
    state_.cameraNum = options.cameraNum.value_or(
        config_media_->GetStillCameraNum() != config_media_->GetCameraSelect()
            ? config_media_->GetStillCameraNum()
            : config_media_->GetCameraSelect());

    //
    // Setting camera parameter configuration from video settings
    //
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
}

bool StillCapture::UninitStillEncoder() {
    MMAL_STATUS_T status = mmal_port_disable(encoder_output_port_);
    mmal_status_to_int(status);

    if (state_.verbose) RTC_LOG(INFO) << "Closing down";

    // Disable all our ports that are not handled by connections
    check_disable_port(camera_video_port_);
    check_disable_port(encoder_output_port_);

    if (state_.preview_connection)
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

    destroy_encoder_still_component(&state_);
    raspipreview_destroy(&state_.preview_parameters);
    destroy_camera_still_component(&state_);

    if (state_.verbose)
        RTC_LOG(INFO) << "Close down completed, all components disconnected, "
                         "disabled and destroyed";
    if (status != MMAL_SUCCESS) raspicamcontrol_check_configuration(128);

    return false;
}

StillCapture *StillCapture::mmal_still_capture_ = nullptr;
std::once_flag StillCapture::singleton_flag_;

void StillCapture::createStillCaptureSingleton() {
    mmal_still_capture_ = new StillCapture();
}

StillCapture *StillCapture::Instance() {
    std::call_once(singleton_flag_, createStillCaptureSingleton);
    return mmal_still_capture_;
}

}  // namespace webrtc
