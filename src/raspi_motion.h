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

// to print average delays of processing
// #define PRINT_PROCESS_DELAYS

#include <memory>
#include <vector>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "config_motion.h"
#include "mmal_wrapper.h"
#include "raspi_httpnoti.h"
#include "raspi_motionfile.h"
#include "raspi_motionvector.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/numerics/moving_average.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/clock.h"

class RaspiMotion : public MotionBlobObserver,
                    public MotionImvObserver,
                    public rtc::Event {
   public:
    explicit RaspiMotion(ConfigMotion* config_motion, int width, int height,
                         int framerate, int bitrate);
    explicit RaspiMotion(ConfigMotion* config_motion);
    ~RaspiMotion();

    bool IsEnabled() const;
    bool IsActive() const;

    // Motion Capture will use fixed resolution
    bool StartCapture();
    void StopCapture();

    // Motion Observers
    void OnMotionTriggered(int active_nums) override;
    void OnMotionCleared(int updates) override;
    void OnActivePoints(int total_points, int active_points) override;

    enum MOTION_STATE {
        CLEARED = 0,
        TRIGGERED,
        WAIT_CLEAR,
    };

   private:
    bool DrainProcess();
    // Encoded frame process thread
    rtc::PlatformThread drainThread_;
    webrtc::Mutex drain_lock_;

    bool motion_active_;
    bool motion_drain_quit_;

    // Motion Capture params
    int width_, height_, framerate_, bitrate_;

    webrtc::MMALEncoderWrapper* mmal_encoder_;
    webrtc::Clock* const clock_;

    // motion file
    std::unique_ptr<RaspiMotionFile> motion_file_;

    // making buffer queue_capacity based on IntraFrame Period
    size_t frame_buffer_size_;  // Default Frame buffer size
    size_t mv_buffer_size_;     // Default Frame buffer size

    // MotionVector Analysis
    RaspiMotionVector motion_analysis_;
    MOTION_STATE motion_state_;
    uint64_t last_average_print_timestamp_;
    uint64_t motion_clear_wait_timestamp_;
    uint32_t motion_clear_wait_period_;

    rtc::MovingAverage motion_active_average_;
#ifdef PRINT_PROCESS_DELAYS
    rtc::MovingAverage imv_process_delay_;
    rtc::MovingAverage frame_process_delay_;
    rtc::MovingAverage drain_process_delay_;
#endif  // PRINT_PROCESS_DELAYS
    int motion_active_percent_clear_threshold_;
    int motion_active_percent_trigger_threshold_;
    bool notification_enable_;
    std::string notification_url_;

    ConfigMotion* config_motion_;
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
    rtc::TaskQueue worker_queue_;

    RTC_DISALLOW_COPY_AND_ASSIGN(RaspiMotion);
};

struct RaspiMotionHolder {
    RaspiMotionHolder(ConfigMotion* config_motion);
    ~RaspiMotionHolder();
    bool Start();
    bool Stop();
    bool SetNotificationUrl(bool enable, const std::string url);

   private:
    std::unique_ptr<RaspiMotion> raspi_motion_;
    ConfigMotion* config_motion_;
};

#endif  // RASPI_MOTION_H_
