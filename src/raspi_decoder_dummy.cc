/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include <algorithm>
#include <limits>

#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "raspi_decoder.h"
#include "rtc_base/checks.h"
#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"

// clang-format off
#include "raspi_decoder_dummy.h"
// clang-format on

namespace webrtc {

RaspiDecoderDummy::RaspiDecoderDummy()
    : decoded_image_callback_(nullptr), decoder_initialized_(false) {}

RaspiDecoderDummy::~RaspiDecoderDummy() { Release(); }

int32_t RaspiDecoderDummy::InitDecode(const VideoCodec* codec_settings,
                                      int32_t number_of_cores) {
    if (codec_settings && codec_settings->codecType != kVideoCodecH264) {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    // Release necessary in case of re-initializing.
    int32_t ret = Release();
    if (ret != WEBRTC_VIDEO_CODEC_OK) {
        return ret;
    }

    decoder_config_ = *codec_settings;
    decoder_initialized_ = true;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RaspiDecoderDummy::Release() {
    decoder_initialized_ = false;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RaspiDecoderDummy::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
    decoded_image_callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RaspiDecoderDummy::Decode(const EncodedImage& input_image,
                                  bool /*missing_frames*/,
                                  int64_t render_time_ms) {
    if (!IsInitialized()) {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (!decoded_image_callback_) {
        RTC_LOG(LS_WARNING)
            << "InitDecode() has been called, but a callback function "
               "has not been set with RegisterDecodeCompleteCallback()";
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (!input_image.data() || !input_image.size()) {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    VideoFrame frame(
        I420Buffer::Create(decoder_config_.width, decoder_config_.height),
        kVideoRotation_0, render_time_ms * rtc::kNumMicrosecsPerMillisec);
    frame.set_timestamp(input_image.Timestamp());
    frame.set_ntp_time_ms(input_image.ntp_time_ms_);

    decoded_image_callback_->Decoded(frame);

    return WEBRTC_VIDEO_CODEC_OK;
}

const char* RaspiDecoderDummy::ImplementationName() const {
    return "RaspiDecoderDummy";
}

bool RaspiDecoderDummy::IsInitialized() const { return decoder_initialized_; }

}  // namespace webrtc
