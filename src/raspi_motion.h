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

#ifndef RASPI_MOTION_H_
#define RASPI_MOTION_H_

#include <memory>
#include <vector>

#include "mmal_wrapper.h"
#include "raspi_httpnoti.h"
#include "raspi_motionfile.h"
#include "raspi_motionvector.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/numerics/moving_average.h"
#include "rtc_base/platform_thread.h"
#include "system_wrappers/include/clock.h"

class RaspiMotion : public MotionBlobObserver,
                    public MotionImvObserver,
                    public rtc::Event {
   public:
    explicit RaspiMotion(int width, int height, int framerate, int bitrate);
    explicit RaspiMotion();
    ~RaspiMotion();

    bool IsActive() const;

    // Motion Capture will use fixed resolution
    bool StartCapture();
    void StopCapture();

    // Motion Observers
    virtual void OnMotionTriggered(int active_nums) override;
    virtual void OnMotionCleared(int updates) override;
    virtual void OnActivePoints(int total_points, int active_points) override;

    enum MOTION_STATE {
        CLEARED = 0,
        TRIGGERED,
        WAIT_CLEAR,
    };

   private:
    bool motion_active_;
    bool motion_drain_quit_;
    static void DrainThread(void*);
    bool DrainProcess();

    bool motion_vector_quit_;
    static void MotionVectorThread(void*);
    bool MotionVectorProcess();

    // Motion Capture params
    int width_, height_, framerate_, bitrate_;

    webrtc::MMALEncoderWrapper* mmal_encoder_;
    webrtc::Clock* const clock_;

    // buffer for motion vector processing
    std::unique_ptr<rtc::BufferQueue> mv_shared_buffer_;

    // motion file
    std::unique_ptr<RaspiMotionFile> motion_file_;

    // Encoded frame process thread
    bool drainThreadStarted_;
    std::unique_ptr<rtc::PlatformThread> drainThread_;

    // Motion Vector process thread
    bool motionVectorThreadStarted_;
    std::unique_ptr<rtc::PlatformThread> motionVectorThread_;

    // making buffer queue_capacity based on IntraFrame Period
    size_t queue_capacity_;
    size_t frame_queue_size_;  // Default Frame buffer queue size
    size_t mv_queue_size_;     // Default Frame buffer queue size

    // MotionVector Analysis
    RaspiMotionVector motion_analysis_;
    MOTION_STATE motion_state_;
    uint64_t last_average_print_timestamp_;
    uint64_t motion_clear_wait_timestamp_;
    uint32_t motion_clear_wait_period_;

    rtc::MovingAverage motion_active_average_;
    int motion_active_percent_clear_threshold_;
    int motion_active_percent_trigger_threshold_;

    RTC_DISALLOW_COPY_AND_ASSIGN(RaspiMotion);
};

#endif  // RASPI_MOTION_H_
