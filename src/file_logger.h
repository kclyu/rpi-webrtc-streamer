/*
 *  Copyright (c) 2017, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * filelogger.cc
 *
 * The origianl copyright info below.
 */
/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RPI_FILELOGGER_H_
#define RPI_FILELOGGER_H_

#pragma once

#include <assert.h>

#include <string>

#include "rtc_base/log_sinks.h"

namespace utils {

// save the logging messsage to file
class FileLogger {
   public:
    explicit FileLogger(const std::string path,
                        const rtc::LoggingSeverity severity,
                        bool disable_buffering);
    bool Init();
    ~FileLogger();

   private:
    bool DeleteFolderFiles(const std::string &folder);
    bool MoveLogFiletoNextShiftFolder();
    bool MoveLogFiles(const std::string prefix, const std::string &src,
                      const std::string &dest);
    bool inited_;
    std::string dir_path_;
    rtc::LoggingSeverity severity_;
    size_t log_max_file_size_;
    std::unique_ptr<rtc::FileRotatingLogSink> logSink_;
    bool disable_buffering_;
};

};  // namespace utils

#endif  // RPI_FILELOGGER_H_
