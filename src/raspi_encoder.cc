/*
Copyright (c) 2016, rpi-webrtc-streamer Lyu,KeunChang

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

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/system_wrappers/include/metrics.h"

#include "webrtc/common_types.h"
#include "webrtc/common_video/h264/h264_bitstream_parser.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/common_video/h264/profile_level_id.h"

#include "webrtc/base/platform_thread.h"

#include "raspi_encoder.h"

// Maximum supported video encoder resolution.
#define MAX_VIDEO_WIDTH 1296
#define MAX_VIDEO_HEIGHT 972

// Maximum supported video encoder fps.
#define MAX_VIDEO_FPS 30


namespace webrtc {

namespace {

// Used by histograms. Values of entries should not be changed.
enum H264EncoderImplEvent {
    kH264EncoderEventInit = 0,
    kH264EncoderEventError = 1,
    kH264EncoderEventMax = 16,
};
}  // namespace


RaspiEncoderImpl::RaspiEncoderImpl(const cricket::VideoCodec& codec)
    : mmal_encoder_(nullptr),
      encoded_image_callback_(nullptr),
      framedrop_counter_(0),
      max_frame_rate_(0.0f),
      target_bps_(0),
      max_bps_(0),
      mode_(kRealtimeVideo),
      key_frame_interval_(0),
      packetization_mode_(H264PacketizationMode::SingleNalUnit),
      max_payload_size_(0),
      has_reported_init_(false),
      has_reported_error_(false),
      clock_(Clock::GetRealTimeClock()),
      last_keyframe_request_(clock_->TimeInMilliseconds()),
      base_internal_ms_(clock_->TimeInMilliseconds()),
      delta_ntp_internal_ms_(clock_->CurrentNtpInMilliseconds() -
                             clock_->TimeInMilliseconds()),
      last_dropconter_show_(clock_->TimeInMilliseconds()) {
    RTC_CHECK(cricket::CodecNamesEq(codec.name, cricket::kH264CodecName));
    std::string packetization_mode_string;
    if (codec.GetParam(cricket::kH264FmtpPacketizationMode,
                &packetization_mode_string) &&
            packetization_mode_string == "1") {
        packetization_mode_ = H264PacketizationMode::NonInterleaved;
    }
}

RaspiEncoderImpl::~RaspiEncoderImpl() {
    Release();
}

// TODO(kclyu) Because we are using dummyvideocapture, 
// We can not get the correct codec_settings from API 
// Need to find any solution how to fix this issue.
int32_t RaspiEncoderImpl::InitEncode(const VideoCodec* codec_settings,
                                     int32_t number_of_cores,
                                     size_t max_payload_size) {
    ReportInit();
    if (!codec_settings ||
            codec_settings->codecType != kVideoCodecH264) {
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

    // Set internal settings from codec_settings
    // TODO(kclyu) changed the encoder defaultparameter from these
    int	init_width, init_height;

    init_width = width_ = codec_settings->width;
    init_height = height_ = codec_settings->height;
    max_frame_rate_ = static_cast<float>(codec_settings->maxFramerate);
    if( max_frame_rate_ > kMaxRaspiFPS ) max_frame_rate_ = kMaxRaspiFPS;
    mode_ = codec_settings->mode;
    frame_dropping_on_ = codec_settings->H264().frameDroppingOn;
    key_frame_interval_ = codec_settings->H264().keyFrameInterval;
    max_payload_size_ = max_payload_size;

    // Codec_settings uses kbits/second; encoder uses bits/second.
    max_bps_ = codec_settings->maxBitrate ;             // kbps
    if (codec_settings->targetBitrate == 0)
        target_bps_ = codec_settings->startBitrate;     // kbps
    else
        target_bps_ = codec_settings->targetBitrate;    // kbps 

    // Get the instance of MMAL encoder wrapper
    if( (mmal_encoder_ = getMMALEncoderWrapper() ) == nullptr ) {
        LOG(LS_ERROR) << "Failed to get MMAL encoder wrapper";
        ReportError();
        Release();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Overriding initial resolution parameter from codec_settings;
    // init_width = width_ = 1024;  // for testing
    // init_height = height_ = 768;
    init_width = width_ = 640;  
    init_height = height_ = 480;
    max_bps_ = 1700; // kbps
    target_bps_ = 800; // kbps
    max_frame_rate_ = kMaxRaspiFPS;

    LOG(INFO) << "InitEncode request: " << init_width << " x " << init_height;
    if(mmal_encoder_->InitEncoder(init_width, init_height,
                                max_frame_rate_, target_bps_ ) == false ) {
        LOG(LS_ERROR) << "Failed to initialize MMAL encoder";
        Release();
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // start capture in here
    mmal_encoder_->StartCapture();

    // start drain thread ;
    if ( !drainThread_) {
        LOG(INFO) << "MMAL encoder drain thread initialized.";
        drainThread_.reset(new rtc::PlatformThread(
                               RaspiEncoderImpl::DrainThread, this, "DrainThread"));
        drainThread_->Start();
        drainThread_->SetPriority(rtc::kHighPriority);
        drainStarted_ = true;
    }

    return WEBRTC_VIDEO_CODEC_OK;
}


int32_t RaspiEncoderImpl::Release() {
    LOG(INFO) << "RaspiEncoder Release request: ";

    if (drainThread_) {
        drainStarted_ = false;
        // Make sure the drain thread stop using the critsect.
        drainThread_->Stop();
        drainThread_.reset();
    }

    if (mmal_encoder_) {
        // unInitializeMMAL return value does have no meaning
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
        const BitrateAllocation& bitrate_allocation, uint32_t framerate) {
    
    if (bitrate_allocation.get_sum_bps() <= 0 || framerate <= 0)
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;

    if( max_frame_rate_ != static_cast<float>(framerate) ) {
        max_frame_rate_ = static_cast<float>(framerate);
        if( max_frame_rate_ >= kMaxRaspiFPS )
            max_frame_rate_ = kMaxRaspiFPS;
        else
            LOG(INFO) << "Changing frame rate : "  << framerate ;
    }

    if( target_bps_!= bitrate_allocation.get_sum_bps() ) {
        target_bps_ = bitrate_allocation.get_sum_bps();
        if(target_bps_ > max_bps_ ) 
            target_bps_ = max_bps_;
        else
            LOG(INFO) << "Changing bitrate : " << target_bps_ ;
    }
    mmal_encoder_->SetRate( static_cast<uint32_t>(max_frame_rate_), target_bps_ );
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
        LOG(LS_WARNING) << "InitEncode() has been called, but a callback function "
                        << "has not been set with RegisterEncodeCompleteCallback()";
        ReportError();
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    bool force_key_frame = false;
    if (frame_types != nullptr) {
        // We only support a single stream.
        RTC_DCHECK_EQ(frame_types->size(), static_cast<size_t>(1));
        // Skip frame?
        if ((*frame_types)[0] == kEmptyFrame) {
            return WEBRTC_VIDEO_CODEC_OK;
        }
        // Force key frame?
        force_key_frame = (*frame_types)[0] == kVideoFrameKey;
    }

    if (force_key_frame) {
        uint32_t kKeyFrameAllowedInterval = 3000;   
        // function forces a key frame regardless of the |bIDR| argument's value.
        // (If every frame is a key frame we get lag/delays.)
        if( clock_->TimeInMilliseconds() - last_keyframe_request_ > kKeyFrameAllowedInterval ){
            mmal_encoder_->ForceKeyFrame();
            last_keyframe_request_ = clock_->TimeInMilliseconds();
        }
    }
    // No more thing to do
    return WEBRTC_VIDEO_CODEC_OK;
}


bool RaspiEncoderImpl::DrainThread(void* obj)
{
    return static_cast<RaspiEncoderImpl *> (obj)->DrainProcess();
}

bool RaspiEncoderImpl::DrainProcess()
{
    // leave the CritSect untouched
    // drainCritSect_->Enter();
    // drainCritSect_->Leave();
    int32_t callback_status = 0;
    MMAL_BUFFER_HEADER_T *buf = nullptr;

    // frame queue is empty?
    if( mmal_encoder_->Length() == 0 ) {
        return true;
    }

    buf = mmal_encoder_->DequeueFrame();
    if (encoded_image_callback_ && buf && buf->length > 0 ) {
        CodecSpecificInfo codec_specific;
        uint32_t fragment_index;
        std::vector<NalUnit> nal_unit_list;
        int qp;

        // Split encoded image up into fragments. This also updates |encoded_image_|.
        RTPFragmentationHeader frag_header;
        memset( &frag_header, 0x00, sizeof(frag_header));

        h264_stream_parser_.ParseBitstream(buf->data, buf->length, &nal_unit_list);

        if ( nal_unit_list.size() == 0 ) {
            // could not find the nal unit in the buffer, so do nothing.
            LOG(INFO) <<  "NAL unit length is zero!!!" ;
            if( buf ) mmal_encoder_->ReturnToPool(buf);
            return true;
        };

        if ( frame_dropping_on_ && drop_next_frame_ ) {
            const uint64_t kDropCounterShow = 3000;

            // keep Key or config frame from dropping 
            if( !(buf->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME ||
                    buf->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) )  {
                framedrop_counter_++;
                if( clock_->TimeInMilliseconds() - last_dropconter_show_ > kDropCounterShow ) {
                    LOG(INFO) <<  "Frame dropped: " << framedrop_counter_;
                }
                drop_next_frame_ = false;   // reset frame drop flag
                if( buf ) mmal_encoder_->ReturnToPool(buf);
                return true;
            }
        }

        encoded_image_._length = buf->length;
        encoded_image_._buffer = buf->data;
        encoded_image_._size = buf->alloc_size;
        encoded_image_._completeFrame = true;
        encoded_image_._encodedWidth = mmal_encoder_->getWidth();
        encoded_image_._encodedHeight = mmal_encoder_->getHeight();

        int64_t capture_ntp_time_ms;
        int64_t current_time = clock_->TimeInMilliseconds();
        capture_ntp_time_ms = current_time + delta_ntp_internal_ms_;
        const int kMsToRtpTimestamp = 90;

// #define __INTERNAL_TIMESTAMP__
#ifdef __INTERNAL_TIMESTAMP__
        // LOG(INFO) << "PTS: " << buf->pts << ", Current: " << clock_->TimeInMicroseconds();
        // Convert NTP time, in ms, to RTP timestamp.
        encoded_image_._timeStamp = kMsToRtpTimestamp * 
            static_cast<uint32_t>(capture_ntp_time_ms);
        encoded_image_.ntp_time_ms_ = capture_ntp_time_ms;
        encoded_image_.capture_time_ms_ = current_time;
#endif

        encoded_image_._timeStamp = kMsToRtpTimestamp * 
            static_cast<uint32_t>(capture_ntp_time_ms-base_internal_ms_);
        encoded_image_.ntp_time_ms_ = capture_ntp_time_ms;
        encoded_image_.capture_time_ms_ = current_time;

        encoded_image_._frameType =
            (buf->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)? kVideoFrameKey: kVideoFrameDelta;


        // Each NAL unit is a fragment starting with the four-byte start code {0,0,0,1}.
        // All of this encoded data already in the encoded_image->_buffer which is filled by MMAL encoder.
        // Fragmentize will count the nal start code in the encoded_image and
        // will mark the the frag_header for fragmentation.
        frag_header.VerifyAndAllocateFragmentationHeader(nal_unit_list.size());

        fragment_index = 0;
        for( std::vector<NalUnit>::const_iterator it =  nal_unit_list.begin();
                it != nal_unit_list.end(); it++, fragment_index++ ) {
            frag_header.fragmentationOffset[fragment_index] = it->offset_;
            frag_header.fragmentationLength[fragment_index] = it->length_;
        }

        codec_specific.codecType = kVideoCodecH264;
        codec_specific.codecSpecific.H264.packetization_mode = packetization_mode_;

        // Deliver encoded image.
        EncodedImageCallback::Result result =
            encoded_image_callback_->OnEncodedImage(encoded_image_, &codec_specific, &frag_header);
        if( result.drop_next_frame == true ) {
            // Frame dropping should be used only for i420 image dropping. 
            // H.264 frame dropping cause serious video damage and problem.
            // drop_next_frame_ = true;
        }
    }

    if( buf ) mmal_encoder_->ReturnToPool(buf);
    if( mmal_encoder_->Length() == 0 ) usleep(1000);

    // TODO: if encoded_size is zero, we need to reset encoder itself
    return true;
}

const char* RaspiEncoderImpl::ImplementationName() const {
    return "RASPIH264";
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

int32_t RaspiEncoderImpl::SetChannelParameters( uint32_t packet_loss, int64_t rtt) {
    // LOG(INFO) << "Packet Loss: " << packet_loss << " , RTT: " << rtt;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RaspiEncoderImpl::SetPeriodicKeyFrames(bool enable) {
    LOG(INFO) << "Periodic Key Frame request: "
              << static_cast<const char *>(enable?"enable":"disable");
    return WEBRTC_VIDEO_CODEC_OK;
}

VideoEncoder::ScalingSettings RaspiEncoderImpl::GetScalingSettings() const {
  return VideoEncoder::ScalingSettings(true);
}

MMALVideoEncoderFactory::MMALVideoEncoderFactory() {
    supported_codecs_.clear();
    LOG(INFO) << "Raspberry H.264 MMAL encoder factory.";

    cricket::VideoCodec constrained_baseline(cricket::kH264CodecName);
    // TODO(magjed): Enumerate actual level instead of using hardcoded level
    // 3.1. Level 3.1 is 1280x720@30fps which is enough for now.
    const webrtc::H264::ProfileLevelId constrained_baseline_profile(
            webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1);
    constrained_baseline.SetParam(
            cricket::kH264FmtpProfileLevelId,
            *webrtc::H264::ProfileLevelIdToString(constrained_baseline_profile));
    constrained_baseline.SetParam(cricket::kH264FmtpLevelAsymmetryAllowed, "1");
    constrained_baseline.SetParam(cricket::kH264FmtpPacketizationMode, "1");
    supported_codecs_.push_back(constrained_baseline);
}

MMALVideoEncoderFactory::~MMALVideoEncoderFactory() {
    LOG(INFO) << "MMALVideoEncoderFactory destroy";
}

VideoEncoder* MMALVideoEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
    if (supported_codecs_.empty()) {
        LOG(INFO) << "No HW video encoder for codec " << codec.name;
        return nullptr;
    }
    if (FindMatchingCodec(supported_codecs_, codec)) {
        LOG(INFO) << "Create Raspberry PI HW MMAL video encoder for " << codec.name;
        return new RaspiEncoderImpl( codec );
    }
    LOG(LS_ERROR)  << "Can not find HW video encoder for type " << codec.name;
    return nullptr;
}

bool MMALVideoEncoderFactory::EncoderTypeHasInternalSource(VideoCodecType type) const {
    if (supported_codecs_.empty()) {
        LOG(LS_ERROR) << "Internal Error, H264 codec spec not added.";
        return false;
    };
    return true;
}

const std::vector<cricket::VideoCodec>& MMALVideoEncoderFactory::supported_codecs() const {
  return supported_codecs_;
}

void MMALVideoEncoderFactory::DestroyVideoEncoder(webrtc::VideoEncoder* encoder) {
    LOG(INFO) << "Destroy video encoder.";
    delete encoder;
}


}  // namespace webrtc
