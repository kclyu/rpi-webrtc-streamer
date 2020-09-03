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

#include "raspi_encoder_impl.h"

#include <limits>
#include <string>

#include "absl/strings/match.h"
#include "common_types.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/profile_level_id.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "config_media.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "raspi_encoder.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

// Used by histograms. Values of entries should not be changed.
enum H264EncoderImplEvent {
    kH264EncoderEventInit = 0,
    kH264EncoderEventError = 1,
    kH264EncoderEventMax = 16,
};
}  // namespace

static const int kDelayForStackCalmDown = 300;
static const uint32_t kKeyFrameAllowedInterval = 3000;  // 3 secs

///////////////////////////////////////////////////////////////////////////////
//
// Raspi Encoder Impl
//
///////////////////////////////////////////////////////////////////////////////
RaspiEncoderImpl::RaspiEncoderImpl(const cricket::VideoCodec& codec)
    : mmal_encoder_(nullptr),
      config_media_(nullptr),
      has_reported_init_(false),
      has_reported_error_(false),
      encoded_image_callback_(nullptr),
      clock_(Clock::GetRealTimeClock()),
      last_keyframe_request_(clock_->TimeInMilliseconds()),

      mode_(VideoCodecMode::kRealtimeVideo),
      max_payload_size_(0),
      key_frame_interval_(0),
      packetization_mode_(H264PacketizationMode::SingleNalUnit) {
    RTC_CHECK(absl::EqualsIgnoreCase(codec.name, cricket::kH264CodecName));
    std::string packetization_mode_string;
    if (codec.GetParam(cricket::kH264FmtpPacketizationMode,
                       &packetization_mode_string) &&
        packetization_mode_string == "1") {
        packetization_mode_ = H264PacketizationMode::NonInterleaved;
    }

    config_media_ = ConfigMediaSingleton::Instance();
}

RaspiEncoderImpl::~RaspiEncoderImpl() { Release(); }

int32_t RaspiEncoderImpl::InitEncode(const VideoCodec* codec_settings,
                                     const VideoEncoder::Settings& settings) {
    RTC_LOG(INFO) << __FUNCTION__;
    int framerate_updated;

    ReportInit();
    if (!codec_settings || codec_settings->codecType != kVideoCodecH264) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (codec_settings->maxFramerate == 0) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (codec_settings->width < 1 || codec_settings->height < 1) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    codec_ = *codec_settings;

    //
    // TODO: Need to implement SimulCast related functions
    int number_of_streams =
        SimulcastUtility::NumberOfSimulcastStreams(*codec_settings);
    if (number_of_streams > 1) {
        RTC_LOG(LS_ERROR) << "SimulCast streaming is not implemented";
        return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
    }

    framerate_updated = static_cast<int>(codec_settings->maxFramerate);
    if (config_media_->GetVideoDynamicFps() == false)
        // using fixed fps when use_dynamic_video_fps is disabled
        framerate_updated = config_media_->GetFixedVideoFps();

    if (framerate_updated > 30) framerate_updated = 30;
    quality_config_.ReportFrameRate(framerate_updated);

    mode_ = codec_settings->mode;
    key_frame_interval_ = codec_settings->H264().keyFrameInterval;
    max_payload_size_ = settings.max_payload_size;

    // frame dropping is not used...
    frame_dropping_on_ = codec_settings->H264().frameDroppingOn;

    // Codec_settings uses kbits/second; encoder uses bits/second.
    quality_config_.ReportTargetBitrate(codec_settings->startBitrate);

    // Get the instance of MMAL encoder wrapper
    if ((mmal_encoder_ = MMALWrapper::Instance()) == nullptr) {
        RTC_LOG(LS_ERROR) << "Failed to get MMAL encoder wrapper";
        ReportError();
        Release();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Set media config params
    mmal_encoder_->SetMediaConfigParams();

    // clear InlineMotionVectors enable flag
    mmal_encoder_->SetInlineMotionVectors(false);

    // Settings for Quality
    // GetInitialBestMatch should be used only when initializing
    // the Encoder, and only when the use_default_resolution flag is on.
    QualityConfig::Resolution initial_res;
    quality_config_.GetInitialBestMatch(initial_res);

    RTC_LOG(INFO) << "InitEncode request: " << initial_res.width_ << " x "
                  << initial_res.height_;
    if (mmal_encoder_->encoder_initdelay_.InitEncoder(
            initial_res.width_, initial_res.height_, initial_res.framerate_,
            initial_res.bitrate_) == false) {
        Release();
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Find the largest video resolution among the resolutions registered
    // in the video resolution, and set the maximum EncodedImageBufer capacity
    // to the frame buffer.
    int max_width = 0, max_height = 0;
    config_media_->GetMaxVideoResolution(max_width, max_height);
    if (max_width * max_height == 0)
        RTC_LOG(LS_ERROR) << "Invalid Max Video Width or Height"
                          << "Width: " << max_width << "Height: " << max_height;
    int required_capacity = max_width * max_height * 0.85;
    RTC_LOG(INFO) << "********************** EncodedImageBuffer capacity: "
                  << required_capacity;
    encoded_image_.SetEncodedData(
        EncodedImageBuffer::Create(required_capacity));

    // start capture in here
    mmal_encoder_->StartCapture();

    // start drain thread ;
    if (!drainThread_) {
        drainQuit_ = false;
        RTC_LOG(INFO) << "MMAL encoder drain thread initialized.";
        drainThread_.reset(
            new rtc::PlatformThread(RaspiEncoderImpl::DrainThread, this,
                                    "DrainThread", rtc::kHighPriority));
        drainThread_->Start();
        drainStarted_ = true;
    }

    SimulcastRateAllocator init_allocator(codec_);
    VideoBitrateAllocation allocation =
        init_allocator.Allocate(VideoBitrateAllocationParameters(
            DataRate::KilobitsPerSec(codec_.startBitrate),
            codec_.maxFramerate));
    SetRates(RateControlParameters(allocation, codec_.maxFramerate));
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RaspiEncoderImpl::Release() {
    if (drainThread_) {
        drainQuit_ = true;
        drainStarted_ = false;
        drainThread_->Stop();
        drainThread_.reset();
    }

    if (mmal_encoder_) {
        mmal_encoder_->StopCapture();
        mmal_encoder_->UninitEncoder();
        mmal_encoder_ = nullptr;
    }
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RaspiEncoderImpl::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
    encoded_image_callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

void RaspiEncoderImpl::SetRates(const RateControlParameters& parameters) {
    QualityConfig::Resolution resolution;
    int target_bitrate = parameters.bitrate.get_sum_bps() / 1000;  // kbps
    uint32_t framerate = static_cast<uint32_t>(parameters.framerate_fps);

    if (mmal_encoder_ == nullptr) {
        RTC_LOG(LS_WARNING) << "SetRates() while uninitialized.";
        return;
    }

    if (parameters.framerate_fps < 1.0) {
        RTC_LOG(LS_WARNING)
            << "Invalid frame rate: " << parameters.framerate_fps;
        return;
    }

    if (target_bitrate == 0) {
        // Encoder paused, turn off all frame sending to native stack.
        frame_flow_.Clear();
        RTC_LOG(INFO)
            << "Required bitrate is 0, Stopping encoded frame sending";
        return;
    }

    if (frame_flow_.IsEnabled() == false) {
        frame_flow_.Set();
        RTC_LOG(INFO) << "Start to send encoded frame. bitrate: "
                      << target_bitrate;
    }

    if (config_media_->GetVideoDynamicFps() == true) {
        // using dynamic fps, so update fps when required
        if (framerate > 30) framerate = 30;
    } else
        // using fixed fps when use_dynamic_video_fps is disabled
        framerate = config_media_->GetFixedVideoFps();

    quality_config_.ReportFrameRate(static_cast<int>(framerate));
    quality_config_.ReportTargetBitrate(target_bitrate);
    if (quality_config_.GetBestMatch(resolution) == true) {
        RTC_LOG(INFO) << "Resolution Changing by Bitrate Changing "
                      << "To : " << resolution.width_ << "x"
                      << resolution.height_;

        if (mmal_encoder_->encoder_initdelay_.ReinitEncoder(
                resolution.width_, resolution.height_, resolution.framerate_,
                resolution.bitrate_) == false) {
            RTC_LOG(LS_ERROR) << "Failed to reinit MMAL encoder";
        }
    } else
        mmal_encoder_->SetRate(static_cast<uint32_t>(framerate),
                               target_bitrate);
}

int32_t RaspiEncoderImpl::Encode(
    const VideoFrame& frame, const std::vector<VideoFrameType>* frame_types) {
    if (!IsInitialized()) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (!encoded_image_callback_) {
        RTC_LOG(LS_WARNING)
            << "InitEncode() has been called, but a callback function "
            << "has not been set with RegisterEncodeCompleteCallback()";
        ReportError();
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    bool send_key_frame = false;
    if (frame_types != nullptr) {
        // We only support a single stream.
        RTC_DCHECK_EQ(frame_types->size(), static_cast<size_t>(1));
        // Skip frame?
        if ((*frame_types)[0] == VideoFrameType::kEmptyFrame) {
            return WEBRTC_VIDEO_CODEC_OK;
        }  // Force key frame?

        if ((*frame_types)[0] == VideoFrameType::kVideoFrameKey &&
            frame_flow_.IsEnabled()) {
            send_key_frame = true;
        }
    }

    if (send_key_frame) {
        // function forces a key frame regardless of the |bIDR| argument's
        // value. (If every frame is a key frame we get lag/delays.)
        if (clock_->TimeInMilliseconds() - last_keyframe_request_ >
            kKeyFrameAllowedInterval) {
            mmal_encoder_->SetForceNextKeyFrame();
            last_keyframe_request_ = clock_->TimeInMilliseconds();
            RTC_LOG(INFO) << "KeyFrame requested.";
        }
    }
    // No more thing to do
    return WEBRTC_VIDEO_CODEC_OK;
}

bool RaspiEncoderImpl::IsInitialized() const {
    return mmal_encoder_ != nullptr;
}

void RaspiEncoderImpl::ReportInit() {
    if (has_reported_init_) return;
    RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264EncoderImpl.Event",
                              kH264EncoderEventInit, kH264EncoderEventMax);
    has_reported_init_ = true;
}

void RaspiEncoderImpl::ReportError() {
    if (has_reported_error_) return;
    RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264EncoderImpl.Event",
                              kH264EncoderEventError, kH264EncoderEventMax);
    has_reported_error_ = true;
}

VideoEncoder::EncoderInfo RaspiEncoderImpl::GetEncoderInfo() const {
    EncoderInfo info;
    info.supports_native_handle = false;
    info.implementation_name = "RaspiEncoder";
    info.has_trusted_rate_controller = true;
    info.is_hardware_accelerated = true;
    info.has_internal_source = true;
    info.supports_simulcast = false;
    return info;
}

///////////////////////////////////////////////////////////////////////////////
//
// Raspi Encoder Frame Flow Control
//
///////////////////////////////////////////////////////////////////////////////
RaspiEncoderImpl::FrameFlowCtl::FrameFlowCtl()
    : clock_(Clock::GetRealTimeClock()) {
    // Get the instance of MMAL encoder wrapper
    if ((mmal_encoder_ = MMALWrapper::Instance()) == nullptr) {
        RTC_LOG(LS_ERROR) << "Failed to get MMAL encoder wrapper";
    }
}

void RaspiEncoderImpl::FrameFlowCtl::Set() {
    if (flow_state_ != FLOW_DISABLED) return;  // no need to change
    RTC_LOG(INFO) << "RaspiEncoder changing Frame flow changed to enable: ";
    flow_state_ = FLOW_WAITING_KEYFRAME;
    keyframe_wait_start_time_ = clock_->TimeInMilliseconds();
}

void RaspiEncoderImpl::FrameFlowCtl::Clear() {
    if (flow_state_ == FLOW_DISABLED) return;  // no need to change
    RTC_LOG(INFO) << "RaspiEncoder changing frame flow changed to disable: ";
    flow_state_ = FLOW_DISABLED;
    keyframe_wait_start_time_ = 0;
}

bool RaspiEncoderImpl::FrameFlowCtl::IsEnabled() {
    if (flow_state_ != FLOW_DISABLED)
        return true;
    else
        return false;
}

bool RaspiEncoderImpl::FrameFlowCtl::CheckKeyframe(bool is_keyframe) {
    if (IsEnabled() == false) return false;
    if (flow_state_ == FLOW_ENABLED)
        // no need to go through below logic
        return true;
    if (is_keyframe == true) {
        flow_state_ = FLOW_ENABLED;
        return true;
    }

    if ((clock_->TimeInMilliseconds() - keyframe_wait_start_time_) >
        kDelayForStackCalmDown) {
        if (flow_state_ == FLOW_WAITING_KEYFRAME) {
            flow_state_ = FLOW_KEYFRAME_REQUSTED;
            mmal_encoder_->SetForceNextKeyFrame();
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// Raspi Encoder MMAL frame drain processing
//
///////////////////////////////////////////////////////////////////////////////
void RaspiEncoderImpl::DrainThread(void* obj) {
    RaspiEncoderImpl* raspi_encoder = static_cast<RaspiEncoderImpl*>(obj);
    while (raspi_encoder->DrainProcess()) {
    };
}

bool RaspiEncoderImpl::DrainProcess() {
    MMAL_BUFFER_HEADER_T* buf = nullptr;

    if (drainQuit_ == true) return false;  // quit drain thread

    //  The GetEncodedFrame function will wait in block state
    //  until there is a new buf or timeout.
    buf = mmal_encoder_->GetEncodedFrame();

    // encoded_image_callback must be registered before pass
    // the frame to WebRTC native stack.
    // If it is timout, buf will have null.
    // In addition, only the normal frame is transmitted to the WebRTC
    // native stack, and the Motion Vector(CODECSIDEINFO) is not
    // transmitted.
    //
    // TODO: if encoded_size is zero, we need to reset encoder itself
    if (encoded_image_callback_ && buf && buf->length > 0 &&
        !(buf->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO)
        // && frame_flow_.CheckKeyframe(buf->flags &
        //                           MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
    ) {
        CodecSpecificInfo codec_specific;
        int qp;

        // Parsing h264 frame
        h264_bitstream_parser_.ParseBitstream(buf->data, buf->length);
        if (h264_bitstream_parser_.GetLastSliceQp(&qp)) {
            encoded_image_.qp_ = qp;
        };

        // Search the NAL unit in the stream
        const std::vector<H264::NaluIndex> nalu_indexes =
            H264::FindNaluIndices(buf->data, buf->length);

        if (nalu_indexes.empty()) {
            // could not find the nal unit in the buffer, so do nothing.
            RTC_LOG(INFO) << "NAL unit length is zero!!!";
            if (buf) mmal_encoder_->ReleaseFrame(buf);
            return true;
        };

        RTC_DCHECK_GT(buf->alloc_size, buf->length)
            << "Internal Error, Frame Queue Bufer size invalid";

        // In native code, there is DCHECK-related logic for capture_time
        // and ntp_time. Currently, RWS does not use video frame capture and
        // encoding, so DCHECK generates an error message in time.
        // the '-10' value is for the part to prevent this.
        // it is tempoary measure, but it happens the '-10' will be
        // removed when this issue fixed
        int64_t capture_time_ms = clock_->TimeInMilliseconds() - 10;
        int64_t ntp_capture_time_ms = clock_->CurrentNtpInMilliseconds() - 10;

        encoded_image_._encodedWidth = mmal_encoder_->GetWidth();
        encoded_image_._encodedHeight = mmal_encoder_->GetHeight();
        encoded_image_.SetTimestamp(capture_time_ms);
        encoded_image_.ntp_time_ms_ = ntp_capture_time_ms;
        encoded_image_.capture_time_ms_ = capture_time_ms;

        // encoded_image_.set_buffer(buf->data, buf->alloc_size);
        memcpy(encoded_image_.data(), buf->data, buf->length);
        encoded_image_.set_size(buf->length);
        encoded_image_._frameType =
            (buf->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
                ? VideoFrameType::kVideoFrameKey
                : VideoFrameType::kVideoFrameDelta;

        // SimulCast is not implemented
        encoded_image_.SetSpatialIndex(0);

        codec_specific.codecType = kVideoCodecH264;
        codec_specific.codecSpecific.H264.packetization_mode =
            packetization_mode_;
        codec_specific.codecSpecific.H264.temporal_idx = kNoTemporalIdx;
        codec_specific.codecSpecific.H264.idr_frame =
            encoded_image_._frameType == VideoFrameType::kVideoFrameKey;
        codec_specific.codecSpecific.H264.base_layer_sync = false;

        // Deliver encoded image.
        EncodedImageCallback::Result result =
            encoded_image_callback_->OnEncodedImage(encoded_image_,
                                                    &codec_specific);
        if (result.error == EncodedImageCallback::Result::ERROR_SEND_FAILED) {
            RTC_LOG(LS_ERROR) << "Error in passng EncodedImage";
        }
        quality_config_.ReportFrame(buf->length);
    }

    if (buf) mmal_encoder_->ReleaseFrame(buf);
    return true;
}

}  // namespace webrtc
