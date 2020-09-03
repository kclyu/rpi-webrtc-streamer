/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMPAT_FILESTREAM_H_
#define COMPAT_FILESTREAM_H_

#include <stdio.h>

#include <memory>

#include "rtc_base/buffer.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// FileStream is a simple implementation of a StreamInterface, which does not
// support asynchronous notification.
///////////////////////////////////////////////////////////////////////////////

// TODO(bugs.webrtc.org/6463): Delete this class.
class FileStream : public StreamInterface {
   public:
    FileStream();
    ~FileStream() override;

    // The semantics of filename and mode are the same as stdio's fopen
    virtual bool Open(const std::string& filename, const char* mode,
                      int* error);
    virtual bool OpenShare(const std::string& filename, const char* mode,
                           int shflag, int* error);

    // By default, reads and writes are buffered for efficiency.  Disabling
    // buffering causes writes to block until the bytes on disk are updated.
    virtual bool DisableBuffering();

    StreamState GetState() const override;
    StreamResult Read(void* buffer, size_t buffer_len, size_t* read,
                      int* error) override;
    StreamResult Write(const void* data, size_t data_len, size_t* written,
                       int* error) override;
    void Close() override;
    virtual bool SetPosition(size_t position);

    bool Flush() override;

   protected:
    virtual void DoClose();

    FILE* file_;

   private:
    RTC_DISALLOW_COPY_AND_ASSIGN(FileStream);
};

}  // namespace rtc

#endif  // COMPAT_FILESTREAM_H_
