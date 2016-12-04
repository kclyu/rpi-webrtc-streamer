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

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/include/metrics.h"
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


RaspiEncoderImpl::RaspiEncoderImpl()
    : mmal_encoder_(nullptr),
      encoded_image_callback_(nullptr),
      has_reported_init_(false),
      has_reported_error_(false),
      clock_(Clock::GetRealTimeClock()) {
}

RaspiEncoderImpl::~RaspiEncoderImpl() {
    Release();
}


int32_t RaspiEncoderImpl::InitEncode(const VideoCodec* codec_settings,
                                     int32_t number_of_cores,
                                     size_t /*max_payload_size*/) {
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

    // the initial codec setting from user media constraints
    // TODO(kclyu) : dummyvideocapture not passing these codec settings.
    codec_settings_ = *codec_settings;
    if (codec_settings_.targetBitrate == 0)
        codec_settings_.targetBitrate = codec_settings_.startBitrate;

    if(codec_settings_.maxFramerate > MAX_VIDEO_FPS)
        codec_settings_.maxFramerate = MAX_VIDEO_FPS;

    // Get the MMAL encoder wrapper
    if( (mmal_encoder_ = getMMALEncoderWrapper() ) == nullptr ) {
        LOG(LS_ERROR) << "Failed to get MMAL encoder wrapper";
        ReportError();
        Release();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

#ifdef _0
    if(mmal_encoder_->InitEncoder(codec_settings_.width, codec_settings_.height,
                                  codec_settings_.maxFramerate,
                                  codec_settings_.startBitrate) == false ) {
        LOG(LS_ERROR) << "Failed to initialize MMAL encoder";
        Release();
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
#endif

    int	init_width, init_height;
    // automatic resolution scale enabled by default
    resolution_scale_ = true;

    // initial resolution and rate parameters for QualityScaler initialization
    codec_settings_.width = init_width = 1024;
    codec_settings_.height = init_height = 768;
    codec_settings_.minBitrate = 400; // kilobits/sec.
    codec_settings_.maxBitrate = 1700; // kilobits/sec.
    codec_settings_.targetBitrate = 800; // kilobits/sec.

    LOG(INFO) << "InitEncode request: " << init_width << " x " << init_height;
#ifdef RASPI_QUALITY
    if(resolution_scale_ ) {
        quality_scaler_.Init(RaspiQualityScaler::kLowH264QpThreshold,
                             RaspiQualityScaler::kBadH264QpThreshold,
                             codec_settings_.startBitrate,
                             init_width, init_height,
                             codec_settings_.maxFramerate);

        RaspiQualityScaler::Resolution res = quality_scaler_.GetScaledResolution();
        res_width_ = init_width = res.width;
        res_width_ = init_height = res.height;
        LOG(INFO) << "Scaled resolution: " << init_width << " x " << init_height;
    }
#endif // RASPI_QUALITY

    if(mmal_encoder_->InitEncoder(init_width, init_height,
                                  codec_settings_.maxFramerate,
                                  codec_settings_.startBitrate) == false ) {
        LOG(LS_ERROR) << "Failed to initialize MMAL encoder";
        Release();
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // start capture in here
    mmal_encoder_->StartCapture();

    // start drain thread ;
    // TODO(kclyu):  check std::unique_ptr<> is really necessary?
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

int32_t RaspiEncoderImpl::SetRates(uint32_t bitrate, uint32_t framerate) {
    if (bitrate <= 0 || framerate <= 0) {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    framerate = (framerate > MAX_VIDEO_FPS)? MAX_VIDEO_FPS:framerate;
    if( codec_settings_.maxFramerate != framerate ) {
        LOG(INFO) << "Changing framerate : " << codec_settings_.maxFramerate <<  " to " << framerate;
        codec_settings_.maxFramerate = framerate;
#ifdef RASPI_QUALITY
        if (resolution_scale_) {
            quality_scaler_.ReportFramerate(framerate);
        }
#endif // RASPI_QUALITY
    }

    if( codec_settings_.targetBitrate != bitrate ) {
        LOG(INFO) << "Changing bitrate : " << codec_settings_.targetBitrate << " to " << bitrate ;
        codec_settings_.targetBitrate = bitrate;
    }

    mmal_encoder_->SetRate( framerate, bitrate );
    return WEBRTC_VIDEO_CODEC_OK;
}


int32_t RaspiEncoderImpl::Encode(
    const VideoFrame& frame, const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
    if (!IsInitialized()) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (frame.IsZeroSize()) {
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (!encoded_image_callback_) {
        LOG(LS_WARNING) << "InitEncode() has been called, but a callback function "
                        << "has not been set with RegisterEncodeCompleteCallback()";
        ReportError();
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (frame.width()  != codec_settings_.width ||
            frame.height() != codec_settings_.height) {
        LOG(LS_WARNING) << "Encoder initialized for " << codec_settings_.width
                        << "x" << codec_settings_.height << " but trying to encode "
                        << frame.width() << "x" << frame.height() << " frame.";
        ReportError();
        return WEBRTC_VIDEO_CODEC_ERR_SIZE;
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
        // API doc says ForceIntraFrame(false) does nothing, but calling this
        // function forces a key frame regardless of the |bIDR| argument's value.
        // (If every frame is a key frame we get lag/delays.)
        // openh264_encoder_->ForceIntraFrame(true);
        LOG(INFO) << "MMAL encoder keyframe requested.";
        mmal_encoder_->ForceKeyFrame();
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

        if (drop_next_frame_) {
            drop_next_frame_ = false;
            // if( buf ) mmal_encoder_->ReturnToPool(buf);
            // return true;
        }

#ifdef RASPI_QUALITY
        if ( resolution_scale_) {
            if (h264_stream_parser_.GetLastSliceQp(&qp)) {
                quality_scaler_.ReportQP(qp);
            }
            // Check framerate before spatial resolution change.
            quality_scaler_.OnEncodeFrame(mmal_encoder_->getWidth(), mmal_encoder_->getHeight());
            const webrtc::RaspiQualityScaler::Resolution scaled_resolution =
                quality_scaler_.GetScaledResolution();
            /***
            if (scaled_resolution.width != codec_settings_.width ||
            		scaled_resolution.height != codec_settings_.height ) {
            	LOG(INFO) << "Quality scaler changed its resolution "
            		<< scaled_resolution.width << "x" << scaled_resolution.height;
            }
            *****/
            if (scaled_resolution.width != res_width_ ||
                    scaled_resolution.height != res_height_ ) {
                LOG(INFO) << "Quality scaler changed its resolution "
                          << scaled_resolution.width << "x" << scaled_resolution.height
                          << "res "
                          << res_width_ << "x" << res_height_;
            }
            res_width_ = scaled_resolution.width ;
            res_height_ = scaled_resolution.height;
        }
#endif // RASPI_QUALITY

        encoded_image_._length = buf->length;
        encoded_image_._buffer = buf->data;
        encoded_image_._size = buf->alloc_size;
        encoded_image_._completeFrame = true;
        encoded_image_._encodedWidth = mmal_encoder_->getWidth();
        encoded_image_._encodedHeight = mmal_encoder_->getHeight();

        // mark timestamp as current time
        encoded_image_._timeStamp = static_cast<uint32_t>(clock_->TimeInMilliseconds());
        // int64_t TimeInMilliseconds()
        encoded_image_.ntp_time_ms_ = clock_->TimeInMilliseconds();
        encoded_image_.capture_time_ms_ = clock_->CurrentNtpInMilliseconds();
        // int64_t CurrentNtpInMilliseconds()

        encoded_image_._frameType =
            (buf->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)? kVideoFrameKey: kVideoFrameDelta;

#ifdef RASPI_QUALITY
        encoded_image_.adapt_reason_.quality_resolution_downscales =
            resolution_scale_ ? quality_scaler_.downscale_shift() : -1;
#else
        encoded_image_.adapt_reason_.quality_resolution_downscales = -1;
#endif // RASPI_QUALITY

        codec_specific.codecType = kVideoCodecH264;


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

        // Deliver encoded image.
        EncodedImageCallback::Result result =
            // Changing API from branch-heads/56
            //  . 'Encoded'--> 'OnEncodedImage' from 
            //  . EncodedImageCallback Result 
            //  . OnDroppedFrame move to EncodedImageCallback
            encoded_image_callback_->OnEncodedImage(encoded_image_, &codec_specific, &frag_header);
        if( result.drop_next_frame == true ) {
            LOG(INFO) << "OnEncodedImage request dropping next frame";
            drop_next_frame_ = true;
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
    // TODO(kclyu) check scaling setting parameter action
    return VideoEncoder::ScalingSettings(false);
}

MMALVideoEncoderFactory::MMALVideoEncoderFactory() {
    supported_codecs_.clear();

    LOG(INFO) << "Raspberry H.264 MMAL encoder factory.";
    supported_codecs_.push_back(VideoCodec(kVideoCodecH264, "H264",
                                           MAX_VIDEO_WIDTH, MAX_VIDEO_HEIGHT, MAX_VIDEO_FPS));
}

MMALVideoEncoderFactory::~MMALVideoEncoderFactory() {
    LOG(INFO) << "MMALVideoEncoderFactory destroy";
}

VideoEncoder* MMALVideoEncoderFactory::CreateVideoEncoder(VideoCodecType type) {

    if (supported_codecs_.empty()) {
        LOG(LS_ERROR) << "Internal Error, H264 codec spec not added.";
        return nullptr;
    }
    for (std::vector<VideoCodec>::const_iterator it = supported_codecs_.begin();
            it != supported_codecs_.end(); ++it) {
        if (it->type == type) {
            LOG(INFO) << "Create HW Accelerated video encoder for type " << (int)type <<
                      " (" << it->name << ").";
            return new RaspiEncoderImpl;
        }
    }
    LOG(LS_ERROR) << "Create RaspEncoder failed. NOT REACHEDED";
    return nullptr;
}

bool MMALVideoEncoderFactory::EncoderTypeHasInternalSource(VideoCodecType type) const {
    if (supported_codecs_.empty()) {
        LOG(LS_ERROR) << "Internal Error, H264 codec spec not added.";
        return false;
    };

    for (std::vector<VideoCodec>::const_iterator it = supported_codecs_.begin();
            it != supported_codecs_.end(); ++it) {
        if (it->type == type) {
            LOG(INFO) << "Using internal source type for MMAL encoder" << (int)type <<
                      " (" << it->name << ").";
            return true;
        }
    }
    LOG(LS_ERROR) << "Create RaspEncoder failed to set InternalSource";
    return false;
}

const std::vector<MMALVideoEncoderFactory::VideoCodec>&
MMALVideoEncoderFactory::codecs() const {
    return supported_codecs_;
}

void MMALVideoEncoderFactory::DestroyVideoEncoder(webrtc::VideoEncoder* encoder) {
    LOG(INFO) << "Destroy video encoder.";
    delete encoder;
}



}  // namespace webrtc
