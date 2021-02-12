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

#include "config_media.h"
#include "frame_queue.h"
#include "mmal_video.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

////////////////////////////////////////////////////////////////////////////////
//
// Encoder Init Delay
//
////////////////////////////////////////////////////////////////////////////////

//
// PASS  : Initial Status
//      There will be no delay/waiting to do reinit
//      condition: time difference should be over kDelayDurationBetweenReinitMs
//
// DELAY : making kDelayDurationBetweenReinitMs
//       After kDelayDurationBetweenReinitMs, fire reinit by TaskQueue
//      condition: time difference should be within
//      kDelayDurationBetweenReinitMs
//
// WAITING: making kDelayDurationBetweenReinitMs
//       After kDelayDurationBetweenReinitMs, status will be PASS
//
//
class MMALEncoderWrapper;  // forward
enum EncoderDelayedInitStatus { INIT_PASS = 1, INIT_DELAY, INIT_WAITING };

struct EncoderDelayedInit {
    explicit EncoderDelayedInit(MMALEncoderWrapper *mmal_encoder);
    ~EncoderDelayedInit();

    bool InitEncoder(int width, int height, int framerate, int bitrate);
    bool ReinitEncoder(int width, int height, int framerate, int bitrate);
    bool UpdateStatus();

   private:
    class DelayInitTask;
    Clock *const clock_;
    uint64_t last_init_timestamp_ms_;
    EncoderDelayedInitStatus status_;
    MMALEncoderWrapper *mmal_encoder_;

    DelayInitTask *delayinit_task_;
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

    bool IsInited();

    int GetWidth();
    int GetHeight();

    bool InitEncoder(int width, int height, int framerate, int bitrate);
    bool UninitEncoder();

    // It is used when reinitialization is required after InitEncoder.
    // and used when changing the resolution.
    // TODO: merge the ReinitEncoder to SetParam and Reinit Internal
    bool ReinitEncoder(int width, int height, int framerate, int bitrate);

    // Used in EncoderDelayInit. Init parameters should be initialized first
    // and then Init must be done, so the two functions are separated.
    bool SetEncodingParams(int width, int height, int framerate, int bitrate);
    bool ReinitEncoderInternal();

    bool StartCapture();
    bool StopCapture();

    bool SetRate(int framerate, int bitrate);

    bool IsDigitalZoomActive();
    bool IncreaseDigitalZoom(double cx, double cy);
    bool DecreaseDigitalZoom();
    bool ResetDigitalZoom();
    bool MoveDigitalZoom(double cx, double cy);

    // When there is a KeyFrame request, it requests the MMAL to generate a key
    // frame. MMAL generates a key frame, and then operates as it is currently
    // set.
    bool SetForceNextKeyFrame();

    void SetIntraPeriod(int frame_period);
    void SetInlineMotionVectors(bool motion_enable);
    void SetVideoRotation(int rotation);
    void SetVideoROI(wstreamer::VideoRoi roi);
    void SetVideoFlip(bool vflip, bool hflip);
    void SetVideoAnnotate(bool annotate_enable);
    void SetVideoAnnotateUserText(const std::string user_text);
    void SetVideoAnnotateTextSize(const int text_size);
    void SetVideoAnnotateTextSizeRatio(const int text_ratio);

    // Video Image related parameter settings
    void SetVideoSharpness(const int sharpness);
    void SetVideoContrast(const int contrast);
    void SetVideoBrightness(const int brightness);
    void SetVideoSaturation(const int saturation);
    void SetVideoEV(const int ev);
    void SetVideoExposureMode(const std::string exposure_mode);
    void SetVideoFlickerMode(const std::string flicker_mode);
    void SetVideoAwbMode(const std::string awb_mode);
    void SetVideoDrcMode(const std::string drc_mode);
    void SetVideoVideoStabilisation(bool stab_enable);

    // Set the necessary media config information.
    void SetVideoConfigParams(void);

    // Callback Functions
    void OnBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    static void BufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    size_t GetBufferSize() const;
    size_t GetBufferNum() const;

    EncoderDelayedInit encoder_initdelay_;

   private:
    size_t GetRecommandedBufferSize(MMAL_PORT_T *port);
    size_t GetRecommandedBufferNum(MMAL_PORT_T *port);
    void CheckCameraConfig();
    bool mmal_initialized_;

    MMAL_PORT_T *camera_preview_port_;
    MMAL_PORT_T *camera_video_port_;
    MMAL_PORT_T *camera_still_port_;
    MMAL_PORT_T *preview_input_port_;
    MMAL_PORT_T *encoder_input_port_;
    MMAL_PORT_T *encoder_output_port_;
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
