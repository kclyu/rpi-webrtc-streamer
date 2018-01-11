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

#ifndef RASPI_MOTIONFIE_H_
#define RASPI_MOTIONFIE_H_

#include <memory>

#include "rtc_base/platform_thread.h"

#include "rtc_base/file.h"
#include "rtc_base/bufferqueue.h"

class RaspiMotionFile : public rtc::Event {
public:
    explicit RaspiMotionFile(const std::string base_path,const std::string prefix,
            int queue_capacity, int frame_queue_size, int motion_queue_size);
    ~RaspiMotionFile();

    // Frame queuing
    bool FrameQueuing(const void* data, size_t bytes, 
            size_t* bytes_written, bool is_keyframe);
    // Inline Motion Vector queuing
    bool ImvQueuing(const void* data, size_t bytes, 
            size_t* bytes_written, bool is_keyframe);


    // Clear the frame and imv buffer
    void QueueClear(void);
    size_t FrameQueueSize(void) const;
    size_t ImvQueueSize(void) const;

    bool WriterStatus(void);
    bool StartWriterThread(void);
    bool StopWriterThread(void);

    bool ManagingVideoFolder(void);

private:
    static int  kEventWaitPeriod;  // minimal wait period between frame
    const std::string GetDateTimeString(void);
    bool OpenWriterFiles(void);
    bool CloseWriterFiles(void);
    bool ImvFileWrite(void);
    bool FrameFileWrite(void);


    static bool WriterThread(void*);
    bool WriterProcess();

    std::string base_path_;
    std::string prefix_;

    // Frame and MV queue for saving frame and mv
    std::unique_ptr<rtc::BufferQueue> frame_queue_;
    std::unique_ptr<rtc::BufferQueue> imv_queue_;

    // thread for file writing
    bool writerThreadStarted_;
    std::unique_ptr<rtc::PlatformThread> writerThread_;

    rtc::File frame_file_;
    rtc::File imv_file_;
    size_t total_frame_written_size_;
    size_t frame_file_size_limit_;
    uint32_t frame_counter_;
    uint32_t imv_counter_;

    rtc::Pathname h264_filename_;
    rtc::Pathname h264_temp_;

    uint8_t *frame_writer_buffer_;

    rtc::CriticalSection crit_sect_;
    RTC_DISALLOW_COPY_AND_ASSIGN(RaspiMotionFile);
};

#endif  // RASPI_MOTIONFIE_H_
