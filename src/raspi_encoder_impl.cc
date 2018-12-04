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

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/mediaconstants.h"
#include "system_wrappers/include/metrics.h"

#include "common_types.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/profile_level_id.h"

#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "modules/video_coding/utility/simulcast_utility.h"

#include "rtc_base/platform_thread.h"

#include "config_media.h"

#include "raspi_encoder.h"
#include "raspi_encoder_impl.h"


namespace webrtc {

namespace {

// Used by histograms. Values of entries should not be changed.
enum H264EncoderImplEvent {
    kH264EncoderEventInit = 0,
    kH264EncoderEventError = 1,
    kH264EncoderEventMax = 16,
};
}  // namespace

// QP scaling thresholds.
static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 37;


///////////////////////////////////////////////////////////////////////////////
//
// Raspi Encoder Impl
//
///////////////////////////////////////////////////////////////////////////////
RaspiEncoderImpl::RaspiEncoderImpl(const cricket::VideoCodec& codec)
    : start_encoding_(false), mmal_encoder_(nullptr), config_media_(nullptr),
    has_reported_init_(false), has_reported_error_(false),
    encoded_image_callback_(nullptr),
    clock_(Clock::GetRealTimeClock()),
    delta_ntp_internal_ms_(clock_->CurrentNtpInMilliseconds() -
            clock_->TimeInMilliseconds()),
    base_internal_ms_(clock_->TimeInMilliseconds()),
    last_keyframe_request_(clock_->TimeInMilliseconds()),

    mode_(VideoCodecMode::kRealtimeVideo), max_payload_size_(0),
    key_frame_interval_(0),
    packetization_mode_(H264PacketizationMode::SingleNalUnit),
    initial_delay_(0) {

    RTC_CHECK(absl::EqualsIgnoreCase(codec.name, cricket::kH264CodecName));
    std::string packetization_mode_string;
    if (codec.GetParam(cricket::kH264FmtpPacketizationMode,
            &packetization_mode_string) && packetization_mode_string == "1") {
        packetization_mode_ = H264PacketizationMode::NonInterleaved;
    }

    config_media_ = ConfigMediaSingleton::Instance();
}

RaspiEncoderImpl::~RaspiEncoderImpl() {
    Release();
}

int32_t RaspiEncoderImpl::InitEncode(const VideoCodec* inst,
                                     int32_t number_of_cores,
                                     size_t max_payload_size) {
    RTC_LOG(INFO) << __FUNCTION__;
    int framerate_updated;

    ReportInit();
    if (!inst || inst->codecType != kVideoCodecH264) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (inst->maxFramerate == 0) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (inst->width < 1 || inst->height < 1) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    //
    // TODO: Need to implement SimulCast related functions
    //
    int number_of_streams = SimulcastUtility::NumberOfSimulcastStreams(*inst);
    if(number_of_streams > 1) {
        RTC_LOG(LS_ERROR) << "SimulCast streaming is not implemented";
        return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
    }

    framerate_updated = static_cast<int>(inst->maxFramerate);
    if( config_media_->GetVideoDynamicFps() == false)
        // using fixed fps when use_dynamic_video_fps is disabled
        framerate_updated = config_media_->GetFixedVideoFps();

    if( framerate_updated > 30 ) framerate_updated = 30;
    quality_config_.ReportFrameRate( framerate_updated);

    mode_ = inst->mode;
    key_frame_interval_ = inst->H264().keyFrameInterval;
    max_payload_size_ = max_payload_size;

    // frame dropping is not used...
    frame_dropping_on_ = inst->H264().frameDroppingOn;

    // Codec_settings uses kbits/second; encoder uses bits/second.
    quality_config_.ReportMaxBitrate(inst->maxBitrate);
    if (inst->targetBitrate == 0)
        quality_config_.ReportTargetBitrate(inst->startBitrate);
    else
        quality_config_.ReportTargetBitrate(inst->targetBitrate);

    // Get the instance of MMAL encoder wrapper
    if( (mmal_encoder_ = MMALWrapper::Instance() ) == nullptr ) {
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
    quality_config_.GetInitialBestMatch( initial_res );


    RTC_LOG(INFO) << "InitEncode request: "
        << initial_res.width_ << " x " << initial_res.height_;
    if(mmal_encoder_->encoder_initdelay_.InitEncoder(initial_res.width_,
            initial_res.height_, initial_res.framerate_,
            initial_res.bitrate_) == false ) {
        Release();
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // start capture in here
    mmal_encoder_->StartCapture();

    // start drain thread ;
    if ( !drainThread_) {
        RTC_LOG(INFO) << "MMAL encoder drain thread initialized.";
        drainThread_.reset(new rtc::PlatformThread(
                               RaspiEncoderImpl::DrainThread, this, "DrainThread"));
        drainThread_->Start();
        drainThread_->SetPriority(rtc::kHighPriority);
        drainStarted_ = true;
    }

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RaspiEncoderImpl::Release() {
    RTC_LOG(INFO) << "RaspiEncoder Release request: ";

    if (drainThread_) {
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

int32_t RaspiEncoderImpl::SetRateAllocation(
        const VideoBitrateAllocation& bitrate_allocation, uint32_t framerate) {
    QualityConfig::Resolution resolution;
    uint32_t framerate_updated;

    if( config_media_->GetVideoDynamicFps() == true ) {
        // using dynamic fps, so update fps when required
        if( framerate > 30 )
            framerate_updated = 30;
        else
            framerate_updated = framerate;
    }
    else
        // using fixed fps when use_dynamic_video_fps is disabled
        framerate_updated = config_media_->GetFixedVideoFps();

    if (bitrate_allocation.get_sum_bps() <= 0 || framerate <= 0)
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

    quality_config_.ReportFrameRate(static_cast<int>(framerate_updated) );
    quality_config_.ReportTargetBitrate( bitrate_allocation.get_sum_kbps());
    if( quality_config_.GetBestMatch( resolution) ) {
        RTC_LOG(INFO) << "Resolution Changing by Bitrate Changing "
            << "To : "  << resolution.width_ << "x" << resolution.height_;

        if(mmal_encoder_->encoder_initdelay_.ReinitEncoder(resolution.width_,
                    resolution.height_, framerate_updated,
                    resolution.bitrate_) == false ) {
            RTC_LOG(LS_ERROR) << "Failed to reinit MMAL encoder";
        }
    }
    else
        mmal_encoder_->SetRate( static_cast<uint32_t>(framerate_updated),
                bitrate_allocation.get_sum_kbps() );
    return WEBRTC_VIDEO_CODEC_OK;
}


int32_t RaspiEncoderImpl::Encode(
    const VideoFrame& frame, const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {

    if (!IsInitialized()) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (!encoded_image_callback_) {
        RTC_LOG(LS_WARNING) << "InitEncode() has been called, but a callback function "
                        << "has not been set with RegisterEncodeCompleteCallback()";
        ReportError();
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    // When enabled, the drain process transfers the encoded frame to the native stack.
    if( start_encoding_  == false ) start_encoding_ = true;

    bool force_key_frame = false;
    if (frame_types != nullptr) {
        // We only support a single stream.
        RTC_DCHECK_EQ(frame_types->size(), static_cast<size_t>(1));
        // Skip frame?
        if ((*frame_types)[0] == kEmptyFrame) {
            return WEBRTC_VIDEO_CODEC_OK;
        } // Force key frame?
        force_key_frame = (*frame_types)[0] == kVideoFrameKey;
    }

    if (force_key_frame) {
        uint32_t kKeyFrameAllowedInterval = 3000;   // 3 secs
        // function forces a key frame regardless of the |bIDR| argument's value.
        // (If every frame is a key frame we get lag/delays.)
        if( clock_->TimeInMilliseconds() - last_keyframe_request_
                > kKeyFrameAllowedInterval ){
            mmal_encoder_->SetForceNextKeyFrame();
            last_keyframe_request_ = clock_->TimeInMilliseconds();
        }
    }
    // No more thing to do
    return WEBRTC_VIDEO_CODEC_OK;
}

bool RaspiEncoderImpl::IsInitialized() const {
    return mmal_encoder_ != nullptr;
}

void RaspiEncoderImpl::ReportInit() {
    if (has_reported_init_)
        return;
    RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264EncoderImpl.Event",
            kH264EncoderEventInit,
            kH264EncoderEventMax);
    has_reported_init_ = true;
}

void RaspiEncoderImpl::ReportError() {
    if (has_reported_error_)
        return;
    RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.H264EncoderImpl.Event",
            kH264EncoderEventError,
            kH264EncoderEventMax);
    has_reported_error_ = true;
}


///////////////////////////////////////////////////////////////////////////////
//
// Raspi Encoder MMAL frame drain processing
//
///////////////////////////////////////////////////////////////////////////////
bool RaspiEncoderImpl::DrainThread(void* obj) {
    return static_cast<RaspiEncoderImpl *> (obj)->DrainProcess();
}

bool RaspiEncoderImpl::DrainProcess() {
    MMAL_BUFFER_HEADER_T *buf = nullptr;

    //  The GetEncodedFrame function will wait in block state
    //  until there is a new buf or timeout.
    buf = mmal_encoder_->GetEncodedFrame();

    // encoded_image_callback must be registered before pass
    // the frame to WebRTC native stack.
    // If it is timout, buf will have null.
    // In addition, only the normal frame is transmitted to the WebRTC native stack,
    // and the Motion Vector(CODECSIDEINFO) is not transmitted.
    //
    // TODO: if encoded_size is zero, we need to reset encoder itself
    if (encoded_image_callback_ && buf && buf->length > 0 &&
            !( buf->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) ) {
        CodecSpecificInfo codec_specific;
        uint32_t fragment_index;
        // int qp;

        // If the native stack is not ready to receive encoded frame, frame will be dropped.
        if( start_encoding_ == false ) {
            if( buf ) mmal_encoder_->ReleaseFrame(buf);
            return true;
        }

        // Split encoded image up into fragments. This also updates |encoded_image_|.
        RTPFragmentationHeader frag_header;
        memset( &frag_header, 0x00, sizeof(frag_header));

        // Parsing h264 frame
        // currently, RWS do not use QP based quality scale of WebRTC native package
        //
        // h264_bitstream_parser_.ParseBitstream(buf->data, buf->length);
        // if (h264_bitstream_parser_.GetLastSliceQp(&qp)) {
        //    encoded_image_.qp_ = qp;
        // };

        // Search the NAL unit in the stream
        const std::vector<webrtc::H264::NaluIndex> nalu_indexes=
            webrtc::H264::FindNaluIndices(buf->data, buf->length);

        if ( nalu_indexes.empty() ) {
            // could not find the nal unit in the buffer, so do nothing.
            RTC_LOG(INFO) <<  "NAL unit length is zero!!!" ;
            if( buf ) mmal_encoder_->ReleaseFrame(buf);
            return true;
        };

        encoded_image_._length = buf->length;
        encoded_image_._buffer = buf->data;
        encoded_image_._size = buf->alloc_size;
        encoded_image_._completeFrame = true;
        encoded_image_.timing_.flags = VideoSendTiming::kInvalid;
        encoded_image_._encodedWidth = mmal_encoder_->GetWidth();
        encoded_image_._encodedHeight = mmal_encoder_->GetHeight();

        int64_t capture_ntp_time_ms;
        int64_t current_time = clock_->TimeInMilliseconds();
        capture_ntp_time_ms = current_time + delta_ntp_internal_ms_;
        const int kMsToRtpTimestamp = 90;

        encoded_image_.SetTimestamp( kMsToRtpTimestamp *
            static_cast<uint32_t>(capture_ntp_time_ms-base_internal_ms_));
        encoded_image_.ntp_time_ms_ = capture_ntp_time_ms;
        encoded_image_.capture_time_ms_ = current_time;

        encoded_image_._frameType =
            (buf->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
            ? kVideoFrameKey: kVideoFrameDelta;
        if( encoded_image_._frameType  == kVideoFrameKey ) initial_delay_ ++;


        // Each NAL unit is a fragment starting with the four-byte
        // start code {0,0,0,1}.  All of this encoded data already in the
        // encoded_image->_buffer which is filled by MMAL encoder.
        // Fragmentize will count the nal start code in the encoded_image and
        // will mark the the frag_header for fragmentation.
        frag_header.VerifyAndAllocateFragmentationHeader(nalu_indexes.size());

        fragment_index = 0;
        for( std::vector<webrtc::H264::NaluIndex>::const_iterator it
                =  nalu_indexes.begin();
                it != nalu_indexes.end(); it++, fragment_index++ ) {
            frag_header.fragmentationOffset[fragment_index]
                = nalu_indexes[fragment_index].payload_start_offset;
            frag_header.fragmentationLength[fragment_index]
                = nalu_indexes[fragment_index].payload_size;
            frag_header.fragmentationPlType[fragment_index] = 0;
            frag_header.fragmentationTimeDiff[fragment_index] = 0;
        }

        codec_specific.codecType = kVideoCodecH264;
        codec_specific.codecSpecific.H264.packetization_mode = packetization_mode_;

        // SimulCast is not implemented
        encoded_image_.SetSpatialIndex(0);

        // Deliver encoded image.
        EncodedImageCallback::Result result =
            encoded_image_callback_->OnEncodedImage(encoded_image_,
                    &codec_specific, &frag_header);
        if ( result.error == EncodedImageCallback::Result::ERROR_SEND_FAILED){
            RTC_LOG(LS_ERROR) << "Error in passng EncodedImage";
        }
    }

    if( buf ) mmal_encoder_->ReleaseFrame(buf);
    return true;
}

}  // namespace webrtc
