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

#ifndef FRAME_QUEUE_H_
#define FRAME_QUEUE_H_

#include <bitset>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "mmal_video.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

////////////////////////////////////////////////////////////////////////////////
//
// Frame Queue
//
////////////////////////////////////////////////////////////////////////////////
constexpr int kFrameBufferFlagSize = 8;

constexpr int kFrameFlagEOS = 0;
constexpr int kFrameFlagStart = 1;
constexpr int kFrameFlagEnd = 2;
constexpr int kFrameFlagKeyFrame = 3;
constexpr int kFrameFlagConfig = 5;
constexpr int kFrameFlagIdeInfo = 7;  // inline motion vector

struct FrameBuffer {
    explicit FrameBuffer(size_t capacity) : FrameBuffer(capacity, false) {}
    explicit FrameBuffer(size_t capacity, bool temporary) {
        data_ = static_cast<uint8_t *>(malloc(capacity));
        capacity_ = capacity;
        length_ = 0;
        temporary_ = temporary;
    }
    ~FrameBuffer() { free(data_); }

    inline bool isKeyFrame() const { return flags_[kFrameFlagKeyFrame]; }
    inline bool isFrame() const {
        return flags_[kFrameFlagStart] && flags_[kFrameFlagEnd];
    }
    inline bool isFrameEnd() const { return flags_[kFrameFlagEnd]; }
    inline bool isMotionVector() const { return flags_[kFrameFlagIdeInfo]; }
    inline bool isConfig() const { return flags_[kFrameFlagConfig]; }
    inline bool isEOS() const { return flags_[kFrameFlagEOS]; }
    inline void reset() {
        flags_.reset();
        length_ = 0;
    }
    inline size_t length() const { return length_; }
    inline uint8_t *data() const { return data_; }
    inline std::string toString() {
        return flags_.to_string<char, std::string::traits_type,
                                std::string::allocator_type>();
    }
    inline bool isTemporary() const { return temporary_; }
    // disable copy consructor
    FrameBuffer(const FrameBuffer &) = delete;
    FrameBuffer &operator=(const FrameBuffer &) = delete;
    bool copy(const MMAL_BUFFER_HEADER_T *buffer);
    bool append(const MMAL_BUFFER_HEADER_T *buffer);

   private:
    bool temporary_;
    std::bitset<kFrameBufferFlagSize> flags_;
    uint8_t *data_;
    size_t length_;
    size_t capacity_;
};

class FrameQueue : public rtc::Event {
   public:
    explicit FrameQueue();
    explicit FrameQueue(size_t capacity /* number of buffers */,
                        size_t buffer_size /* frame buffer size */);
    ~FrameQueue();

    void Init(size_t capacity, size_t buffer_size);
    // Obtain a frame from the encoded frame queue.
    // If there is no frame in FrameQueue, it will be blocked for a certain
    // period and return nullptr when timeout is reached. If there is a frame,
    // it returns the corresponding frame.
    FrameBuffer *ReadFront(bool wait_until_timeout = true);

    size_t size() const;

   protected:
    void clear();
    // Make the frame uploaded from MMAL into one H.264 frame,
    // and buffering it in the encoded frame of FrameQueue.
    bool WriteBack(MMAL_BUFFER_HEADER_T *buffer);

   private:
    static int kEventWaitPeriod;  // minimal wait period between frame
    webrtc::Mutex mutex_;
    void destroy();

    bool inited_;
    size_t capacity_, buffer_size_;
    std::deque<FrameBuffer *> encoded_frame_queue_;
    std::deque<FrameBuffer *> free_list_;
    std::vector<FrameBuffer *> pending_;

    RTC_DISALLOW_COPY_AND_ASSIGN(FrameQueue);
};

}  // namespace webrtc

#endif  // FRAME_QUEUE_H_
