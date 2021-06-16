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

#include "file_writer_handle.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"
#include "utils.h"

namespace webrtc {

namespace {

constexpr int kWaitPeriodforMotionWriterThread = 10;
constexpr size_t kFileWriterBufferSize = 1024 * 8;
constexpr char kTemporaryFileNameExtension[] = ".saving";

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// File Writer Buffer
//
////////////////////////////////////////////////////////////////////////////////
FileWriterBuffer::FileWriterBuffer(size_t buffer_size)
    : buffer_size_(buffer_size), offset_start_(0), offset_end_(0) {
    buffer_.reset(new uint8_t[buffer_size_]);
    RTC_LOG(INFO) << "FileWriterBuffer buffer size " << buffer_size_;
}

FileWriterBuffer::~FileWriterBuffer() {}

size_t FileWriterBuffer::size() {
    webrtc::MutexLock lock(&mutex_);
    return offset_end_ - offset_start_;
}

void FileWriterBuffer::Resize(double increase_factor) {
    size_t new_buffer_size =
        buffer_size_ + static_cast<size_t>(buffer_size_ * increase_factor);
    uint8_t *new_buffer = new uint8_t[new_buffer_size];
    if (offset_end_ - offset_start_ > 0) {
        // copy
        std::memcpy(new_buffer + offset_start_, buffer_.get() + offset_start_,
                    offset_end_ - offset_start_);
    }
    buffer_.reset(new_buffer);
    RTC_LOG(INFO) << "FileWriterBuffer resizing " << buffer_size_ << " to "
                  << new_buffer_size;
    buffer_size_ = new_buffer_size;
}

size_t FileWriterBuffer::WriteBack(const void *buffer, size_t buffer_size) {
    RTC_DCHECK(offset_end_ >= offset_start_);
    webrtc::MutexLock lock(&mutex_);
    if (offset_end_ + buffer_size > buffer_size_) Resize();
    std::memcpy(buffer_.get() + offset_end_, buffer, buffer_size);
    offset_end_ += buffer_size;  // increase end offset by size
    return buffer_size;
}

void FileWriterBuffer::clear() {
    webrtc::MutexLock lock(&mutex_);
    offset_start_ = offset_end_ = 0;
}

size_t FileWriterBuffer::ReadFront(void *buffer, size_t buffer_size) {
    RTC_DCHECK(offset_end_ >= offset_start_);
    webrtc::MutexLock lock(&mutex_);
    size_t remaining_amount_to_read = offset_end_ - offset_start_;

    if (remaining_amount_to_read > 0) {
        if (remaining_amount_to_read > buffer_size) {
            std::memcpy(buffer, buffer_.get() + offset_start_, buffer_size);
            offset_start_ += buffer_size;
            return buffer_size;
        } else {
            std::memcpy(buffer, buffer_.get() + offset_start_,
                        remaining_amount_to_read);
            // there is noting left
            offset_end_ = offset_start_ = 0;
            return remaining_amount_to_read;
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// File Writer Handle
//
////////////////////////////////////////////////////////////////////////////////
FileWriterHandle::FileWriterHandle(const std::string name, size_t buffer_size)
    : Event(false, false) {
    name_ = name;
    buffer_.reset(new FileWriterBuffer(buffer_size));
}

FileWriterHandle::~FileWriterHandle() { Close(); }

bool FileWriterHandle::Open(const std::string dirname, const std::string prefix,
                            const std::string extension,
                            const int file_size_limit,
                            bool use_temporary_filename) {
    if (utils::IsFolder(dirname) == false) {
        RTC_LOG(LS_ERROR) << "Writer Handle " << name_ << " Error. dirname "
                          << dirname << " is not directory.";
        return false;
    }
    const std::string date_string = utils::GetDateTimeString();
    std::string filename =
        dirname + "/" + prefix + "_" + date_string + extension;

    RTC_DCHECK(writerThread_.empty() == true);

    int open_error = 0;
    if ((file_ = FileWrapper::OpenWriteOnly(
             use_temporary_filename ? filename + kTemporaryFileNameExtension
                                    : filename,
             &open_error))
            .is_open() == false) {
        RTC_LOG(LS_ERROR) << "Writer Handle " << name_
                          << " Failed to open file : " << filename
                          << ", Error: " << open_error;
        return false;
    };

    writerThread_ = rtc::PlatformThread::SpawnJoinable(
        [this] {
            while (WriterProcess()) {
            }
        },
        "WriterThread",
        rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));

    writer_quit_ = false;
    filename_ = filename;
    file_size_limit_ = file_size_limit;
    file_written_ = 0;
    use_temporary_filename_ = use_temporary_filename;
    return true;
}

bool FileWriterHandle::WriterProcess() {
    if (is_open() == false || writer_quit_ == true) {
        return false;
    }

    if (size() == 0) {
        Wait(kWaitPeriodforMotionWriterThread);
    };

    if (is_open() && size()) {
        webrtc::MutexLock lock(&writer_lock_);
        Write();
    }

    return true;
}

bool FileWriterHandle::Close() {
    webrtc::MutexLock lock(&writer_lock_);
    if (file_.is_open()) {
        RTC_LOG(INFO) << "Closing File: " << filename_
                      << " , size: " << file_written_;
        file_.Close();

        // rename the temporary file name to original filename
        if (use_temporary_filename_) {
            utils::MoveFile(filename_ + kTemporaryFileNameExtension, filename_);
        }
    }

    if (!writerThread_.empty()) {
        writer_quit_ = true;
        writerThread_.Finalize();
    }
    filename_.clear();
    file_size_limit_ = file_written_ = 0;
    use_temporary_filename_ = true;  // reset with default value
    return true;
}

bool FileWriterHandle::Write() {
    if (file_.is_open() == false) {
        RTC_LOG(LS_ERROR) << "Writer Handle " << name_
                          << " file handle is not opened.";
        return false;
    }

    uint8_t buf[kFileWriterBufferSize];

    // limit the file size
    if (file_size_limit_ != 0 /* no limit */ &&
        file_written_ > file_size_limit_) {
        // comsume the buffer without file writing
        while (size_t read_size =
                   buffer_->ReadFront(buf, kFileWriterBufferSize)) {
        }
        return true;
    }

    while (size_t read_size = buffer_->ReadFront(buf, kFileWriterBufferSize)) {
        if (file_.Write(buf, read_size) == false) {
            RTC_LOG(LS_ERROR)
                << "Writer Handle " << name_
                << "Failed to write buffer to file : " << filename_;
            return false;
        }
        file_written_ += read_size;
    }
    return true;
}

void FileWriterHandle::Flush() {
    if (file_.is_open()) {
        Write();
        file_.Flush();
    }
}

size_t FileWriterHandle::ReadFront(void *buffer, size_t size) {
    return buffer_->ReadFront(buffer, size);
}

size_t FileWriterHandle::WriteBack(const void *buffer, size_t size) {
    size_t write_back_size = buffer_->WriteBack(buffer, size);
    if (write_back_size != size) {
        RTC_LOG(LS_ERROR)
            << "Internal Error: FileWriterBuffer size mismatching";
        return write_back_size;
    };
    Set();
    return size;
}

void FileWriterHandle::clear() { return buffer_->clear(); }

size_t FileWriterHandle::size() { return buffer_->size(); }

FileWriterBuffer *FileWriterHandle::GetBuffer() { return buffer_.get(); }

}  // namespace webrtc
