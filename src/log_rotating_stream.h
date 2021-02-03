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

#ifndef LOG_ROTATING_STREAM_H_
#define LOG_ROTATING_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "rtc_base/constructor_magic.h"
#include "rtc_base/stream.h"
#include "rtc_base/system/file_wrapper.h"

namespace rtc {

class LogRotatingStream : public StreamInterface {
   public:
    LogRotatingStream(const std::string& dir_path,
                      const std::string& file_prefix, size_t max_file_size,
                      size_t num_files);

    ~LogRotatingStream();

    // StreamInterface methods.
    StreamState GetState() const override;
    StreamResult Read(void* buffer, size_t buffer_len, size_t* read,
                      int* error) override;
    StreamResult Write(const void* data, size_t data_len, size_t* written,
                       int* error) override;
    bool Flush() override;
    void Close() override;

    // Opens the appropriate file(s). Call this before using the stream.
    bool Open();

    // Disabling buffering causes writes to block until disk is updated. This is
    // enabled by default for performance.
    bool DisableBuffering();

    // Returns the path used for the i-th newest file, where the 0th file is the
    // newest file. The file may or may not exist, this is just used for
    // formatting. Index must be less than GetNumFiles().
    std::string GetFilePath(size_t index) const;

    // Returns the number of files that will used by this stream.
    size_t GetNumFiles() const { return file_names_.size(); }

   protected:
    size_t GetMaxFileSize() const { return max_file_size_; }
    void SetMaxFileSize(size_t size) { max_file_size_ = size; }
    size_t GetRotationIndex() const { return rotation_index_; }
    void SetRotationIndex(size_t index) { rotation_index_ = index; }

    virtual void OnRotation() {}

   private:
    bool OpenCurrentFile();
    void CloseCurrentFile();

    // Rotates the files by creating a new current file, renaming the
    // existing files, and deleting the oldest one. e.g.
    // file_0 -> file_1
    // file_1 -> file_2
    // file_2 -> delete
    // create new file_0
    void RotateFiles();

    // Private version of GetFilePath.
    std::string GetFilePath(size_t index, size_t num_files) const;

    std::string dir_path_;
    const std::string file_prefix_;

    // File we're currently writing to.
    webrtc::FileWrapper file_;
    // Convenience storage for file names so we don't generate them over and
    // over.
    std::vector<std::string> file_names_;
    size_t max_file_size_;
    size_t current_file_index_;
    // The rotation index indicates the index of the file that will be
    // deleted first on rotation. Indices lower than this index will be rotated.
    size_t rotation_index_;
    // Number of bytes written to current file. We need this because with
    // buffering the file size read from disk might not be accurate.
    size_t current_bytes_written_;
    bool disable_buffering_;

    RTC_DISALLOW_COPY_AND_ASSIGN(LogRotatingStream);
};

}  // namespace rtc

#endif  // LOG_ROTATING_STREAM_H_
