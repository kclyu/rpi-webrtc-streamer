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

#ifndef FILE_WRITER_HANDLE_H_
#define FILE_WRITER_HANDLE_H_

#include <memory>
#include <mutex>
#include <thread>

#include "rtc_base/constructor_magic.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/thread.h"

namespace webrtc {

constexpr double kDefaultBufferIncreaaseFactor = 0.25;

////////////////////////////////////////////////////////////////////////////////
//
// FileWriter Buffer
//
////////////////////////////////////////////////////////////////////////////////

class FileWriterBuffer {
   public:
    explicit FileWriterBuffer(size_t buffer_size /* frame buffer size */);
    ~FileWriterBuffer();

    size_t ReadFront(void *buffer, size_t size);
    size_t WriteBack(const void *buffer, size_t size);

    size_t size();
    inline size_t buffer_size() const { return buffer_size_; }
    void clear();

   private:
    void Resize(double increase_factor = kDefaultBufferIncreaaseFactor);
    webrtc::Mutex mutex_;

    size_t buffer_size_;
    size_t offset_start_, offset_end_;
    std::unique_ptr<uint8_t[]> buffer_;

    RTC_DISALLOW_COPY_AND_ASSIGN(FileWriterBuffer);
};

////////////////////////////////////////////////////////////////////////////////
//
// FileWriter Handle
//
////////////////////////////////////////////////////////////////////////////////

class FileWriterHandle : public rtc::Event {
   public:
    explicit FileWriterHandle(const std::string name, size_t buffer_size);
    ~FileWriterHandle();

    bool Open(const std::string dirname, const std::string prefix,
              const std::string extension, const int file_size_limit,
              const bool use_temporary_filename = true);
    bool Close();
    bool Write();
    void Flush();
    FileWriterBuffer *GetBuffer();
    inline bool is_open() { return file_.is_open(); }
    inline size_t FileSize() { return file_written_; }

    // interface for buffer
    size_t ReadFront(void *buffer, size_t size);
    size_t WriteBack(const void *buffer, size_t size);
    void clear();   // remove all of contents in file writer buffer
    size_t size();  // file writer buffer size

   private:
    bool WriterProcess();
    std::unique_ptr<FileWriterBuffer> buffer_;
    // thread for file writing
    rtc::PlatformThread writerThread_;
    bool writer_quit_;
    webrtc::Mutex writer_lock_;
    FileWrapper file_;
    std::string filename_;
    int file_size_limit_;
    int file_written_;
    bool use_temporary_filename_;
    std::string name_;

    RTC_DISALLOW_COPY_AND_ASSIGN(FileWriterHandle);
};

}  // namespace webrtc

#endif  // FILE_WRITER_HANDLE_H_
