/*
 * Lyu,KeunChang
 *
 * This is a stripped down version of the original file_rotating_stream module
 * from ther WebRTC native source code,
 * Original copyright info below
 */
/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "log_rotating_stream.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/file_rotating_stream.h"
#include "rtc_base/logging.h"
#include "utils.h"

constexpr char kDefaultLoggingDir[] = INSTALL_DIR "/log";

namespace rtc {

LogRotatingStream::LogRotatingStream(const std::string& dir_path,
                                     const std::string& file_prefix,
                                     size_t max_file_size, size_t num_files)
    : file_prefix_(file_prefix),
      max_file_size_(max_file_size),
      current_file_index_(0),
      current_bytes_written_(0),
      disable_buffering_(false) {
    if (utils::GetFolderWithTailingDelimiter(dir_path, dir_path_) == false) {
        std::fprintf(stderr, "Error in log directory path: %s",
                     dir_path.c_str());
        std::fprintf(stderr, "Using default log path : %s", kDefaultLoggingDir);
        dir_path_ = kDefaultLoggingDir;
    }

    RTC_DCHECK_GT(max_file_size, 0);
    RTC_DCHECK_GT(num_files, 1);
    file_names_.clear();
    for (size_t i = 0; i < num_files; ++i) {
        file_names_.push_back(GetFilePath(i, num_files));
    }
    rotation_index_ = num_files - 1;
}

LogRotatingStream::~LogRotatingStream() {}

StreamState LogRotatingStream::GetState() const {
    return (file_.is_open() ? SS_OPEN : SS_CLOSED);
}

StreamResult LogRotatingStream::Read(void* buffer, size_t buffer_len,
                                     size_t* read, int* error) {
    RTC_DCHECK(buffer);
    RTC_NOTREACHED();
    return SR_EOS;
}

StreamResult LogRotatingStream::Write(const void* data, size_t data_len,
                                      size_t* written, int* error) {
    if (!file_.is_open()) {
        std::fprintf(stderr, "Open() must be called before Write.\n");
        return SR_ERROR;
    }
    // Write as much as will fit in to the current file.
    RTC_DCHECK_LT(current_bytes_written_, max_file_size_);
    size_t remaining_bytes = max_file_size_ - current_bytes_written_;
    size_t write_length = std::min(data_len, remaining_bytes);

    if (!file_.Write(data, write_length)) {
        return SR_ERROR;
    }
    if (disable_buffering_ && !file_.Flush()) {
        return SR_ERROR;
    }

    current_bytes_written_ += write_length;
    if (written) {
        *written = write_length;
    }
    // If we're done with this file, rotate it out.
    if (current_bytes_written_ >= max_file_size_) {
        RTC_DCHECK_EQ(current_bytes_written_, max_file_size_);
        RotateFiles();
    }
    return SR_SUCCESS;
}

bool LogRotatingStream::Flush() {
    if (!file_.is_open()) {
        return false;
    }
    return file_.Flush();
}

void LogRotatingStream::Close() { CloseCurrentFile(); }

bool LogRotatingStream::Open() {
    // rotate the files at the beginning,
    // RotateFiles will open current file to write
    RotateFiles();
    return file_.is_open();
}

bool LogRotatingStream::DisableBuffering() {
    disable_buffering_ = true;
    return true;
}

std::string LogRotatingStream::GetFilePath(size_t index) const {
    RTC_DCHECK_LT(index, file_names_.size());
    return file_names_[index];
}

bool LogRotatingStream::OpenCurrentFile() {
    CloseCurrentFile();

    // Opens the appropriate file in the appropriate mode.
    RTC_DCHECK_LT(current_file_index_, file_names_.size());
    std::string file_path = file_names_[current_file_index_];

    // We should always be writing to the zero-th file.
    RTC_DCHECK_EQ(current_file_index_, 0);

    // rotate files when
    int error;
    file_ = webrtc::FileWrapper::OpenWriteOnly(file_path, &error);
    if (!file_.is_open()) {
        std::fprintf(stderr, "Failed to open: %s Error: %d\n",
                     file_path.c_str(), error);
        return false;
    }
    return true;
}

void LogRotatingStream::CloseCurrentFile() {
    if (!file_.is_open()) {
        return;
    }
    current_bytes_written_ = 0;
    file_.Close();
}

void LogRotatingStream::RotateFiles() {
    CloseCurrentFile();
    // Rotates the files by deleting the file at |rotation_index_|, which is the
    // oldest file and then renaming the newer files to have an incremented
    // index. See header file comments for example.
    RTC_DCHECK_LT(rotation_index_, file_names_.size());
    std::string file_to_delete = file_names_[rotation_index_];
    if (utils::IsFile(file_to_delete)) {
        if (!utils::DeleteFile(file_to_delete)) {
            std::fprintf(stderr, "Failed to delete: %s\n",
                         file_to_delete.c_str());
        }
    }
    for (auto i = rotation_index_; i > 0; --i) {
        std::string rotated_name = file_names_[i];
        std::string unrotated_name = file_names_[i - 1];
        if (utils::IsFile(unrotated_name)) {
            if (!utils::MoveFile(unrotated_name, rotated_name)) {
                std::fprintf(stderr, "Failed to move: %s to %s\n",
                             unrotated_name.c_str(), rotated_name.c_str());
            }
        }
    }
    // Create a new file for 0th index.
    OpenCurrentFile();
    OnRotation();
}

std::string LogRotatingStream::GetFilePath(size_t index,
                                           size_t num_files) const {
    RTC_DCHECK_LT(index, num_files);

    const size_t buffer_size = 32;
    char file_postfix[buffer_size];
    // We want to zero pad the index so that it will sort nicely.
    const int max_digits = std::snprintf(nullptr, 0, "%zu", num_files - 1);
    RTC_DCHECK_LT(1 + max_digits, buffer_size);
    std::snprintf(file_postfix, buffer_size, "_%0*zu", max_digits, index);

    return dir_path_ + file_prefix_ + file_postfix;
}

}  // namespace rtc
