/*
Copyright (c) 2021, rpi-webrtc-streamer Lyu,KeunChang

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

#ifndef MMAL_STILL_CAPTURE_H_
#define MMAL_STILL_CAPTURE_H_

#include "absl/status/status.h"
#include "config_media.h"
#include "mmal_still.h"
#include "mutex"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/file_wrapper.h"
#include "system_wrappers/include/clock.h"
#include "wstreamer_types.h"

namespace webrtc {

////////////////////////////////////////////////////////////////////////////////
//
// MMAL Encoder Wrapper
//
////////////////////////////////////////////////////////////////////////////////
class StillCapture : public rtc::Event {
   public:
    // Singleton, constructor and destructor are private.
    static StillCapture *Instance();
    inline bool IsCapturing() const { return still_capture_active_; }
    absl::Status Capture(const wstreamer::StillOptions &options,
                         std::string *captured_filename = nullptr);
    absl::Status GetLatestOrCapture(const wstreamer::StillOptions &options,
                                    std::string *captured_filename = nullptr);

   private:
    StillCapture();
    ~StillCapture();
    absl::Status InitStillEncoder(const wstreamer::StillOptions &options);
    void InitParams(const wstreamer::StillOptions &options);
    bool UninitStillEncoder();

    // Callback Functions
    static void BufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    void OnBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

    static void createStillCaptureSingleton();
    static StillCapture *mmal_still_capture_;
    static std::once_flag singleton_flag_;

    bool still_capture_active_;

    ConfigMedia *config_media_;
    MMAL_PORT_T *camera_preview_port_, *camera_video_port_, *camera_still_port_;
    MMAL_PORT_T *preview_input_port_;
    MMAL_PORT_T *encoder_input_port_, *encoder_output_port_;
    RASPISTILL_STATE state_;
    FileWrapper file_;

    webrtc::Mutex mutex_;
    RTC_DISALLOW_COPY_AND_ASSIGN(StillCapture);
};

}  // namespace webrtc

#endif  // MMAL_STILL_CAPTURE_H_
