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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef RASPI_ENCODER_H_
#define RASPI_ENCODER_H_

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/codec.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

class RaspiEncoder : public VideoEncoder {
   public:
    static std::unique_ptr<RaspiEncoder> Create(
        const cricket::VideoCodec& codec);
    static bool IsSupported();
    ~RaspiEncoder() override {}
};

std::unique_ptr<VideoEncoderFactory> CreateRaspiVideoEncoderFactory();

//
// Implementation of Raspberry video encoder factory
class RaspiVideoEncoderFactory : public VideoEncoderFactory {
   public:
    RaspiVideoEncoderFactory();
    virtual ~RaspiVideoEncoderFactory() override;

    std::unique_ptr<VideoEncoder> CreateVideoEncoder(
        const SdpVideoFormat& format) override;

    // Returns a list of supported codecs in order of preference.
    std::vector<SdpVideoFormat> GetSupportedFormats() const override {
        return supported_formats_;
    }
    std::vector<SdpVideoFormat> GetImplementations() const override {
        return supported_formats_;
    }

    CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override;

    std::unique_ptr<EncoderSelectorInterface> GetEncoderSelector()
        const override;

   private:
    std::vector<SdpVideoFormat> supported_formats_;
};

}  // namespace webrtc

#endif  // RASPI_ENCODER_H_
