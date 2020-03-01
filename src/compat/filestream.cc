/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <errno.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"

// clang-format off
#include "compat/filestream.h"
// clang-format on

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// FileStream
///////////////////////////////////////////////////////////////////////////////

FileStream::FileStream() : file_(nullptr) {}

FileStream::~FileStream() { FileStream::Close(); }

bool FileStream::Open(const std::string& filename, const char* mode,
                      int* error) {
    Close();
    file_ = fopen(filename.c_str(), mode);
    if (!file_ && error) {
        *error = errno;
    }
    return (file_ != nullptr);
}

bool FileStream::OpenShare(const std::string& filename, const char* mode,
                           int shflag, int* error) {
    Close();
    return Open(filename, mode, error);
}

bool FileStream::DisableBuffering() {
    if (!file_) return false;
    return (setvbuf(file_, nullptr, _IONBF, 0) == 0);
}

StreamState FileStream::GetState() const {
    return (file_ == nullptr) ? SS_CLOSED : SS_OPEN;
}

StreamResult FileStream::Read(void* buffer, size_t buffer_len, size_t* read,
                              int* error) {
    if (!file_) return SR_EOS;
    size_t result = fread(buffer, 1, buffer_len, file_);
    if ((result == 0) && (buffer_len > 0)) {
        if (feof(file_)) return SR_EOS;
        if (error) *error = errno;
        return SR_ERROR;
    }
    if (read) *read = result;
    return SR_SUCCESS;
}

StreamResult FileStream::Write(const void* data, size_t data_len,
                               size_t* written, int* error) {
    if (!file_) return SR_EOS;
    size_t result = fwrite(data, 1, data_len, file_);
    if ((result == 0) && (data_len > 0)) {
        if (error) *error = errno;
        return SR_ERROR;
    }
    if (written) *written = result;
    return SR_SUCCESS;
}

void FileStream::Close() {
    if (file_) {
        DoClose();
        file_ = nullptr;
    }
}

bool FileStream::SetPosition(size_t position) {
    if (!file_) return false;
    return (fseek(file_, static_cast<int>(position), SEEK_SET) == 0);
}

bool FileStream::Flush() {
    if (file_) {
        return (0 == fflush(file_));
    }
    // try to flush empty file?
    RTC_NOTREACHED();
    return false;
}

void FileStream::DoClose() { fclose(file_); }

}  // namespace rtc
