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

#include "file_log_sink.h"

#include <iostream>
#include <memory>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/log_sinks.h"
#include "rtc_base/logging.h"
#include "rtc_base/stream.h"
#include "utils.h"

namespace utils {

namespace {

constexpr int kLogFileSizeLimit = 3 * 1024 * 1024;  // 3M bytes;
constexpr char kLogFilenamePrefix[] = "rws.log";
constexpr size_t kMaxLogFileNums = 10;

}  // namespace

//////////////////////////////////////////////////////////////////////////////////////////
//
// File Logger Class
//
//////////////////////////////////////////////////////////////////////////////////////////
FileLogSink::FileLogSink(const std::string path,
                         const rtc::LoggingSeverity severity,
                         bool disable_buffering)
    : dir_path_(path),
      severity_(severity),
      disable_buffering_(disable_buffering) {
    stream_.reset(new rtc::LogRotatingStream(
        dir_path_, kLogFilenamePrefix, kLogFileSizeLimit, kMaxLogFileNums));
}

FileLogSink::~FileLogSink() {}

bool FileLogSink::Init() {
    if (!stream_->Open()) {
        std::cerr << "Failed to open log files at path: " << dir_path_ << "\n";
        stream_.reset();
        return false;
    }

    if (disable_buffering_ == true) {
        // disabling_buffering in FileLogSink
        stream_->DisableBuffering();
    };

    rtc::LogMessage::LogThreads(true);
    rtc::LogMessage::LogTimestamps(true);
    rtc::LogMessage::AddLogToStream(this, severity_);
    return true;
}

void FileLogSink::OnLogMessage(const std::string& message) {
    if (stream_->GetState() != rtc::SS_OPEN) {
        std::fprintf(stderr,
                     "Init() must be called before adding this sink.\n");
        return;
    }
    stream_->WriteAll(message.c_str(), message.size(), nullptr, nullptr);
}

void FileLogSink::OnLogMessage(const std::string& message,
                               rtc::LoggingSeverity sev, const char* tag) {
    if (stream_->GetState() != rtc::SS_OPEN) {
        std::fprintf(stderr,
                     "Init() must be called before adding this sink.\n");
        return;
    }
    stream_->WriteAll(tag, strlen(tag), nullptr, nullptr);
    stream_->WriteAll(": ", 2, nullptr, nullptr);
    stream_->WriteAll(message.c_str(), message.size(), nullptr, nullptr);
}

};  // namespace utils
