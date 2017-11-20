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
#include "rtc_base/ptr_util.h"

#include "common_types.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/profile_level_id.h"

#include "media/base/codec.h"

#include "raspi_decoder.h"
#include "raspi_decoder_dummy.h"

namespace webrtc {

std::unique_ptr<RaspiDecoder> RaspiDecoder::Create() {
  RTC_LOG(LS_INFO) << "Creating H264DecoderDummy.";
  return rtc::MakeUnique<RaspiDecoderDummy>();
}

bool RaspiDecoder::IsSupported() {
  return true;
}

RaspiVideoDecoderFactory* RaspiVideoDecoderFactory::CreateVideoDecoderFactory() {
  RTC_LOG(LS_INFO) << "Creating RaspiVideoDecoderFactory.";
  return new RaspiVideoDecoderFactory;
}


///////////////////////////////////////////////////////////////////////////////
//
// Raspi Video Decoder Factory
//
///////////////////////////////////////////////////////////////////////////////
RaspiVideoDecoderFactory::RaspiVideoDecoderFactory() {
    RTC_LOG(INFO) << "Raspi H.264 Video decoder factory.";
}

std::unique_ptr<webrtc::VideoDecoder> RaspiVideoDecoderFactory::CreateVideoDecoder(
        const webrtc::SdpVideoFormat& format) {
    const cricket::VideoCodec codec(format);

    // Try creating external decoder.
    std::unique_ptr<webrtc::VideoDecoder> video_decoder;

    RTC_LOG(INFO) << "Raspi " << format.name << " video decoder created";
    video_decoder = RaspiDecoder::Create();
    return move(video_decoder);
}

RaspiVideoDecoderFactory::~RaspiVideoDecoderFactory() {
    RTC_LOG(INFO) << "Raspi Video decoder factory destroy";
}



}  // namespace webrtc
