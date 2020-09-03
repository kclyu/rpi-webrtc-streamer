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

#ifndef RASPI_MOTIONFIE_H_
#define RASPI_MOTIONFIE_H_

#include <ctime>
#include <memory>

#include "compat/file.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/clock.h"

class RaspiMotionFile : public rtc::Event {
   public:
    explicit RaspiMotionFile(const std::string base_path,
                             const std::string prefix, int queue_capacity,
                             int frame_queue_size, int motion_queue_size);
    ~RaspiMotionFile();

    // Frame queuing
    bool FrameQueuing(const void* data, size_t bytes, size_t* bytes_written,
                      bool is_keyframe);
    // Inline Motion Vector queuing
    bool ImvQueuing(const void* data, size_t bytes, size_t* bytes_written,
                    bool is_keyframe);

    bool WriterActive(void);
    bool StartWriter(void);
    bool StopWriter(void);

    bool ManagingVideoFolder(void);
    std::string VideoPathname(void) const;
    std::string VideoFilename(void) const;

   private:
    static int kEventWaitPeriod;  // minimal wait period between frame
    const std::string GetDateTimeString(void);
    bool OpenWriterFiles(void);
    bool CloseWriterFiles(void);
    bool ImvFileWrite(void);
    bool FrameFileWrite(void);

    static void WriterThread(void*);
    bool WriterProcess();

    std::string base_path_;
    std::string prefix_;

    // Frame and MV queue for saving frame and mv
    std::unique_ptr<rtc::BufferQueue> frame_queue_;
    std::unique_ptr<rtc::BufferQueue> imv_queue_;

    // thread for file writing
    bool writer_active_;
    bool writer_quit_;
    std::unique_ptr<rtc::PlatformThread> writerThread_;

    rtc::File frame_file_;
    rtc::File imv_file_;
    size_t total_frame_written_size_;
    size_t frame_file_size_limit_;
    uint32_t frame_counter_;
    uint32_t imv_counter_;

    std::string h264_filename_;
    std::string h264_temp_;

    uint8_t* frame_writer_buffer_;

    webrtc::Mutex mutex_;
    webrtc::Clock* const clock_;
    RTC_DISALLOW_COPY_AND_ASSIGN(RaspiMotionFile);
};

#endif  // RASPI_MOTIONFIE_H_
