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

#ifndef __MMAL_WRAPPER_H__
#define __MMAL_WRAPPER_H__

#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/rtc_base/criticalsection.h"
#include "webrtc/rtc_base/sequenced_task_checker.h"
#include "mmal_encoder.h"

namespace webrtc {

#define FRAME_BUFFER_SIZE	65536*2
#define FRAME_QUEUE_LENGTH 5

////////////////////////////////////////////////////////////////////////////////////////
// 
// Frame Queue
//
////////////////////////////////////////////////////////////////////////////////////////
class FrameQueue {
private:
    int num_, size_;

    MMAL_QUEUE_T *encoded_frame_queue_;
    MMAL_POOL_T *pool_internal_;
    uint8_t		*keyframe_buf_;
    uint8_t		*frame_buf_;
    int			frame_buf_pos_;
    int			frame_segment_cnt_;
    bool		inited_;

    uint32_t	frame_count_;
    uint32_t	frame_drop_;
    uint32_t	frame_queue_drop_;

public:
    FrameQueue();
    ~FrameQueue();
    bool Init();
    void Reset();
    bool Isinitialized();

    // Queueuing and Assembling frame
    void ProcessBuffer( MMAL_BUFFER_HEADER_T *buffer );
    int Length();

    // Buffer header handling
    MMAL_BUFFER_HEADER_T * GetBufferFromPool();
    void ReturnToPool( MMAL_BUFFER_HEADER_T *buffer );
    void QueueingFrame( MMAL_BUFFER_HEADER_T *buffer );
    MMAL_BUFFER_HEADER_T *DequeueFrame();
    void dumpFrameQueueInfo();
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
//      condition: time difference should be within kDelayDurationBetweenReinitMs
//
// WAITING: making kDelayDurationBetweenReinitMs
//       After kDelayDurationBetweenReinitMs, status will be PASS
//
//
class MMALEncoderWrapper;   // forward
enum EncoderDelayedInitStatus {
    INIT_UNKNOWN = 0,       
    INIT_PASS,       
    INIT_DELAY, 
    INIT_WAITING
};

struct EncoderDelayedInit {
    explicit EncoderDelayedInit(MMALEncoderWrapper* mmal_encoder);
    ~EncoderDelayedInit();

    bool InitEncoder(int width, int height, int framerate, int bitrate);
    bool ReinitEncoder(int width, int height, int framerate, int bitrate);
    bool UpdateStatus();

private:
    class DelayInitTask;
    Clock* const clock_;
    uint64_t last_init_timestamp_ms_;
    EncoderDelayedInitStatus status_;
    MMALEncoderWrapper* mmal_encoder_;

    DelayInitTask* delayinit_task_;
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

    int GetWidth();
    int GetHeight();
    bool SetEncodingParams(int width, int height,int framerate, int bitrate );
    bool SetRate(int framerate, int bitrate);
    bool InitEncoder(int width, int height, int framerate, int bitrate);
    bool ReinitEncoder(int width, int height, int framerate, int bitrate );
    bool ReinitEncoderInternal();
    bool UninitEncoder();
    bool ForceKeyFrame();
    bool IsKeyFrame();
    bool StartCapture();
    bool StopCapture();
    bool IsInited();
    void SetVideoRotation(int rotation);
    void SetVideoFlip(bool vflip, bool hflip);

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

    rtc::CriticalSection crit_sect_;
};

// singleton wrappper
MMALEncoderWrapper* getMMALEncoderWrapper();

}	// namespace webrtc

#endif // __MMAL_WRAPPER_H__
