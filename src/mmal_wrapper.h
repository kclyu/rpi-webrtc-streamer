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

#ifndef MMAL_WRAPPER_H_
#define MMAL_WRAPPER_H_

#include "api/task_queue/default_task_queue_factory.h"
#include "config_media.h"
#include "frame_queue.h"
#include "mmal_video.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/clock.h"
#include "wstreamer_types.h"

namespace webrtc {

////////////////////////////////////////////////////////////////////////////////
//
// Encoder Init Delay
//
////////////////////////////////////////////////////////////////////////////////

//
// IDLE  : There will be no delaying/coolingdown to do init
//
// DELAYING : During the coolingdown period, init is requested, and the
//      requested init will be delayed. All init requested during the dellaying
//      period will be also delayed.
//
// COOLINGDOWN: After the encoder init, the encoder init executed within a
//      certain period is delayed. If init is requested within the coolingdown
//      period, the init will be delayed.
//
class MMALEncoderWrapper;  // forward

struct EncoderDelayedInit {
    enum DelayedInitState { IDLE = 1, DELAYING_INIT, INIT_COOLINGDOWN };
    explicit EncoderDelayedInit(MMALEncoderWrapper *mmal_encoder);
    ~EncoderDelayedInit();

    bool InitEncoder(wstreamer::VideoEncodingParams config);
    bool ReinitEncoder(wstreamer::VideoEncodingParams config);
    bool UpdateStateOrMayInit();
    bool IsEncodingActive();
    inline bool IsIdleState() { return state_ == IDLE; }

   private:
    class DelayInitTask;
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
    rtc::TaskQueue task_queue_;
    Clock *const clock_;
    uint64_t last_init_timestamp_ms_;
    DelayedInitState state_;
    MMALEncoderWrapper *mmal_encoder_;
};

////////////////////////////////////////////////////////////////////////////////
//
// MMAL Encoder Wrapper
//
////////////////////////////////////////////////////////////////////////////////
class MMALEncoderWrapper : public FrameQueue {
   public:
    MMALEncoderWrapper();
    ~MMALEncoderWrapper();

    inline bool IsInited() const { return mmal_initialized_; }

    inline int GetEncodingWidth() const { return state_.width; }
    inline int GetEncodingHeight() const { return state_.height; }

    bool InitEncoder(wstreamer::VideoEncodingParams config);
    bool UninitEncoder();

    // It is used when reinitialization is required after InitEncoder.
    // and used when changing the resolution.
    // TODO: merge the ReinitEncoder to SetParam and Reinit Internal
    bool ReinitEncoder(wstreamer::VideoEncodingParams config);
    bool ReinitEncoderInternal();

    bool StartCapture();
    bool StopCapture();

    bool SetRate(int framerate, int bitrate);
    bool Zoom(wstreamer::ZoomOptions options);

    // When there is a KeyFrame request, it requests the MMAL to generate a key
    // frame. MMAL generates a key frame, and then operates as it is currently
    // set.
    bool RequestKeyFrame();

    // Set the necessary media config information.
    void SetEncoderConfigParams(wstreamer::EncoderSettings *params = nullptr);
    // Used in EncoderDelayInit. Init parameters should be initialized first
    // and then Init must be done, so the two functions are separated.
    bool SetEncodingParams(wstreamer::VideoEncodingParams config);

    // Callback Functions
    void OnBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    static void BufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

    EncoderDelayedInit encoder_delayed_init_;

   private:
    size_t GetRecommandedBufferSize(MMAL_PORT_T *port);
    size_t GetRecommandedBufferNum(MMAL_PORT_T *port);
    void CheckCameraConfig();
    bool mmal_initialized_;

    MMAL_PORT_T *camera_preview_port_, *camera_video_port_, *camera_still_port_;
    MMAL_PORT_T *preview_input_port_;
    MMAL_PORT_T *encoder_input_port_, *encoder_output_port_;
    RASPIVID_STATE state_;

    ConfigMedia *config_media_;
    webrtc::Mutex mutex_;
    size_t recommanded_buffer_size_;
    size_t recommanded_buffer_num_;
    RTC_DISALLOW_COPY_AND_ASSIGN(MMALEncoderWrapper);
};

class MMALWrapper {
   public:
    // Singleton, constructor and destructor are private.
    static MMALEncoderWrapper *Instance();

   private:
    static void createMMALWrapperSingleton();
    static MMALEncoderWrapper *mmal_wrapper_;
    static std::once_flag singleton_flag_;

    MMALWrapper();
    ~MMALWrapper();

    RTC_DISALLOW_COPY_AND_ASSIGN(MMALWrapper);
};

}  // namespace webrtc

#endif  // MMAL_WRAPPER_H_
