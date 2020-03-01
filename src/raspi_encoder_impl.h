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

#ifndef RASPI_ENCODER_IMPL_H_
#define RASPI_ENCODER_IMPL_H_

#include <memory>
#include <vector>

#include "common_video/h264/h264_bitstream_parser.h"
#include "mmal_wrapper.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "raspi_encoder.h"
#include "raspi_quality_config.h"
#include "rtc_base/platform_thread.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class RaspiEncoderImpl : public RaspiEncoder {
   public:
    struct FrameFlowCtl {
        enum FLOWCTL_STATE {
            FLOW_DISABLED = 0,
            FLOW_WAITING_KEYFRAME,
            FLOW_KEYFRAME_REQUSTED,
            FLOW_ENABLED
        };
        explicit FrameFlowCtl();
        virtual ~FrameFlowCtl(){};

        Clock* const clock_;
        MMALEncoderWrapper* mmal_encoder_;

        // Change the value to enable when the Encode function is called
        // and wiil be enable when SetRates method called with positive
        // bitrate and will be disable when SetRates methold called with 0
        // bitrate
        FLOWCTL_STATE flow_state_ = FLOW_DISABLED;
        int64_t keyframe_wait_start_time_ = 0;

        void Set();
        void Clear();
        bool IsEnabled(void);
        bool CheckKeyframe(bool keyframe);
    };

   public:
    explicit RaspiEncoderImpl(const cricket::VideoCodec& codec);
    ~RaspiEncoderImpl() override;

    int32_t InitEncode(const VideoCodec* codec_settings,
                       const VideoEncoder::Settings& settings) override;
    int32_t Release() override;

    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override;
    void SetRates(const RateControlParameters& parameters) override;

    // Encode metholds are not used to encode frames. This methold is used only
    // as an event for creating a keyframe in the encoder when a keyframe is
    // needed.
    int32_t Encode(const VideoFrame& frame,
                   const std::vector<VideoFrameType>* frame_types) override;

    VideoEncoder::EncoderInfo GetEncoderInfo() const override;

   private:
    bool IsInitialized() const;

    static void DrainThread(void*);
    bool drainStarted_;
    bool drainQuit_;
    bool DrainProcess();

    // Reports statistics with histograms.
    void ReportInit();
    void ReportError();

    MMALEncoderWrapper* mmal_encoder_;
    // media configuration sigleton reference
    ConfigMedia* config_media_;

    bool has_reported_init_;
    bool has_reported_error_;

    //
    // Encoded frame process thread
    std::unique_ptr<rtc::PlatformThread> drainThread_;

    EncodedImageCallback* encoded_image_callback_;
    EncodedImage encoded_image_;

    // H264 bitstream parser, used to extract QP from encoded bitstreams.
    H264BitstreamParser h264_bitstream_parser_;

    Clock* const clock_;
    int64_t last_keyframe_request_;

    VideoCodecMode mode_;
    size_t max_payload_size_;
    int key_frame_interval_;

    // H.264 specifc parameters
    bool frame_dropping_on_;
    H264PacketizationMode packetization_mode_;

    // Quality Config
    QualityConfig quality_config_;
    VideoCodec codec_;

    // Frame Flow Control
    FrameFlowCtl frame_flow_;
};

}  // namespace webrtc

#endif  // RASPI_ENCODER_IMPL_H_
