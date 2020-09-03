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

#ifndef __MMAL_WRAPPER_H__
#define __MMAL_WRAPPER_H__

#include <mutex>

#include "config_media.h"
#include "mmal_video.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

#define FRAME_BUFFER_SIZE 65536 * 3
#define FRAME_QUEUE_LENGTH 3

////////////////////////////////////////////////////////////////////////////////////////
//
// Frame Queue
//
////////////////////////////////////////////////////////////////////////////////////////

class FrameQueue : public rtc::Event {
   public:
    FrameQueue();
    ~FrameQueue();
    bool Init();
    void Reset();
    bool Isinitialized();

    int Length();

    // Obtain a frame from the encoded frame queue.
    // If there is no frame in FrameQueue, it will be blocked for a certain
    // period and return nullptr when timeout is reached. If there is a frame,
    // it returns the corresponding frame.
    MMAL_BUFFER_HEADER_T *GetEncodedFrame();

    // It returns the used Encoded Frame to the frame pool of QueueFrame.
    void ReleaseFrame(MMAL_BUFFER_HEADER_T *buffer);

   protected:
    // Make the frame uploaded from MMAL into one H.264 frame,
    // and buffering it in the encoded frame of FrameQueue.
    void HandlingMMALFrame(MMAL_BUFFER_HEADER_T *buffer);

   private:
    static int kEventWaitPeriod;  // minimal wait period between frame

    int num_, size_;

    MMAL_QUEUE_T *encoded_frame_queue_;
    MMAL_POOL_T *pool_internal_;
    uint8_t *keyframe_buf_;
    uint8_t *frame_buf_;
    int frame_buf_pos_;
    int frame_segment_cnt_;
    bool inited_;

    uint32_t frame_count_;
    uint32_t frame_drop_;
    uint32_t frame_queue_drop_;
};

////////////////////////////////////////////////////////////////////////////////////////
//
// Encoder Init Delay
//
////////////////////////////////////////////////////////////////////////////////////////

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
enum EncoderDelayedInitStatus {
    INIT_UNKNOWN = 0,
    INIT_PASS,
    INIT_DELAY,
    INIT_WAITING
};

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

////////////////////////////////////////////////////////////////////////////////////////
//
// MMAL Encoder Wrapper
//
////////////////////////////////////////////////////////////////////////////////////////
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
    void SetMediaConfigParams(void);

    // Callback Functions
    void OnBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    static void BufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

    EncoderDelayedInit encoder_initdelay_;

   private:
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

#endif  // __MMAL_WRAPPER_H__
