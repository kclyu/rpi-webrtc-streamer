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

#include "raspi_decoder.h"

#include <limits>
#include <string>

#include "absl/memory/memory.h"
#include "common_types.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/profile_level_id.h"
#include "media/base/codec.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "raspi_decoder_dummy.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

std::unique_ptr<RaspiDecoder> RaspiDecoder::Create() {
    RTC_LOG(LS_INFO) << "Creating H264DecoderDummy.";
    return std::make_unique<RaspiDecoderDummy>();
}

bool RaspiDecoder::IsSupported() { return true; }

std::unique_ptr<VideoDecoderFactory> CreateRaspiVideoDecoderFactory() {
    RTC_LOG(LS_INFO) << "Creating RaspiVideoDecoderFactory.";
    return std::make_unique<RaspiVideoDecoderFactory>();
}

///////////////////////////////////////////////////////////////////////////////
//
// Raspi Video Decoder Factory
//
///////////////////////////////////////////////////////////////////////////////
RaspiVideoDecoderFactory::RaspiVideoDecoderFactory() {
    RTC_LOG(INFO) << "Raspi H.264 Video decoder factory.";
    for (const SdpVideoFormat& h264_format : SupportedH264Codecs())
        supported_formats_.push_back(h264_format);
}

std::unique_ptr<VideoDecoder> RaspiVideoDecoderFactory::CreateVideoDecoder(
    const SdpVideoFormat& format) {
    const cricket::VideoCodec codec(format);

    RTC_LOG(INFO) << "Raspi " << format.name << " video decoder created";
    return RaspiDecoder::Create();
}

RaspiVideoDecoderFactory::~RaspiVideoDecoderFactory() {
    RTC_LOG(INFO) << "Raspi Video decoder factory destroy";
}

std::vector<SdpVideoFormat> RaspiVideoDecoderFactory::GetSupportedFormats()
    const {
    return supported_formats_;
}

}  // namespace webrtc
