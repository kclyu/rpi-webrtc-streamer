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

#ifndef FILE_LOG_SINK_H_
#define FILE_LOG_SINK_H_

#include <memory>
#include <string>

#include "log_rotating_stream.h"
#include "rtc_base/log_sinks.h"

namespace utils {

// save the logging messsage to file
class FileLogSink : public rtc::LogSink {
   public:
    explicit FileLogSink(const std::string path,
                         const rtc::LoggingSeverity severity,
                         bool disable_buffering);
    bool Init();
    ~FileLogSink();

    // Writes the message to the current file. It will spill over to the next
    // file if needed.
    void OnLogMessage(const std::string& message) override;
    void OnLogMessage(const std::string& message, rtc::LoggingSeverity sev,
                      const char* tag) override;

   private:
    std::string dir_path_;
    rtc::LoggingSeverity severity_;
    size_t log_max_file_size_;
    std::unique_ptr<rtc::LogRotatingStream> stream_;
    bool disable_buffering_;
    RTC_DISALLOW_COPY_AND_ASSIGN(FileLogSink);
};

};  // namespace utils

#endif  // FILE_LOG_SINK_H_
