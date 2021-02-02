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

#include "frame_queue.h"

#include <stdio.h>
#include <string.h>

#include "mmal_wrapper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

////////////////////////////////////////////////////////////////////////////////
//
// Frame Buffer
//
////////////////////////////////////////////////////////////////////////////////
bool FrameBuffer::copy(const MMAL_BUFFER_HEADER_T *buffer) {
    RTC_DCHECK(buffer != nullptr)
        << "Internal Error, MMAL Buffer pointer is NULL";
    RTC_DCHECK(buffer->length < capacity_)
        << "Internal Error, Frame Buffer capacity is smaller then buffer "
           "capacity";
    flags_ = buffer->flags;
    std::memcpy(data_.get(), buffer->data, buffer->length);
    length_ = buffer->length;
    // RTC_LOG(INFO) << "Frame copy : " << toString()
    //              << ", size: " << buffer->length;
    return true;
}

bool FrameBuffer::append(const MMAL_BUFFER_HEADER_T *buffer) {
    RTC_DCHECK(buffer != nullptr)
        << "Internal Error, MMAL Buffer pointer is NULL";
    RTC_DCHECK(buffer->length + length_ < capacity_)
        << "Internal Error, Frame Buffer capacity is smaller then buffer "
           "capacity";
    flags_ = buffer->flags;
    std::memcpy(data_.get() + length_, buffer->data, buffer->length);
    length_ += buffer->length;
    // RTC_LOG(INFO) << "Frame append : " << toString()
    //              << ", size: " << buffer->length;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// Frame Queue
//
////////////////////////////////////////////////////////////////////////////////
int FrameQueue::kEventWaitPeriod = 30;  // minimal wait period between frame

FrameQueue::FrameQueue() : Event(false, false), inited_(false) {}

FrameQueue::FrameQueue(size_t capacity, size_t buffer_size)
    : Event(false, false),
      inited_(true),
      capacity_(capacity),
      buffer_size_(buffer_size) {
    for (size_t index = 0; index < capacity_; index++) {
        FrameBuffer *buffer = new FrameBuffer(buffer_size_);
        free_list_.push_back(buffer);
    }
}

void FrameQueue::Init(size_t capacity, size_t buffer_size) {
    capacity_ = capacity;
    buffer_size_ = buffer_size;
    inited_ = true;
    destroy();
    for (size_t index = 0; index < capacity_; index++) {
        FrameBuffer *buffer = new FrameBuffer(buffer_size_);
        free_list_.push_back(buffer);
    }
}

void FrameQueue::destroy() {
    for (FrameBuffer *buffer : pending_) {
        delete buffer;
    }
    pending_.clear();
    for (FrameBuffer *buffer : free_list_) {
        delete buffer;
    }
    free_list_.clear();
    for (FrameBuffer *buffer : encoded_frame_queue_) {
        delete buffer;
    }
    encoded_frame_queue_.clear();
}

FrameQueue::~FrameQueue() { destroy(); }

void FrameQueue::clear() {
    webrtc::MutexLock lock(&mutex_);

    while (!encoded_frame_queue_.empty()) {
        free_list_.push_back(encoded_frame_queue_.front());
        encoded_frame_queue_.pop_front();
    }
}

size_t FrameQueue::size() const { return encoded_frame_queue_.size(); }

FrameBuffer *FrameQueue::ReadFront(bool wait_until_timeout) {
    RTC_DCHECK(inited_ == true);
    if (encoded_frame_queue_.empty() && wait_until_timeout == true)
        Wait(kEventWaitPeriod);  // Waiting for Event or Timeout

    if (encoded_frame_queue_.empty()) return nullptr;

    webrtc::MutexLock lock(&mutex_);
    FrameBuffer *buffer = encoded_frame_queue_.front();
    encoded_frame_queue_.pop_front();
    if (buffer->isTemporary()) {
        // If there is no buffer in the freelist, a buffer is created and used
        // temporarily. These temporary buffers are deleted without being put
        // back into the freelist.
        delete buffer;
    } else
        // keep the frame buffer in free_list again
        free_list_.push_back(buffer);

    return buffer;
}

// Making FrameBuffer from MMAL Frame
bool FrameQueue::WriteBack(MMAL_BUFFER_HEADER_T *mmal_frame) {
    RTC_DCHECK(inited_ == true);
    RTC_DCHECK(mmal_frame != NULL);

    webrtc::MutexLock lock(&mutex_);
    if (free_list_.empty()) {
        // If there is no buffer in the freelist, a buffer is created and used
        // temporarily.
        RTC_LOG(INFO) << "capacity: " << capacity_
                      << ", encoded queue size: " << encoded_frame_queue_.size()
                      << ", free_list size: " << free_list_.size()
                      << ", pending: " << pending_.size();

        FrameBuffer *buffer = new FrameBuffer(buffer_size_, true);
        free_list_.push_back(buffer);
    }
    // ignore the EOS(?)
    if (mmal_frame->length == 0 && mmal_frame->flags == 0) return true;
    if (mmal_frame->length > buffer_size_ || mmal_frame->length < 0) {
        char buffer_log[256];
        RTC_LOG(LS_ERROR) << "**** MMAL Frame size error (buffer size: "
                          << buffer_size_
                          << "), real frame size : " << mmal_frame->length;
        dump_buffer_flag(buffer_log, 256, mmal_frame->flags);
        RTC_LOG(LS_ERROR) << "**** MMAL Flags : " << buffer_log;
        RTC_LOG(LS_ERROR) << "**** MMAL Flags : " << mmal_frame->flags;
        return false;
    }

    FrameBuffer *buffer;
    if (!pending_.empty()) {
        // there is pending frame which need to
        buffer = pending_.back();
        if (buffer->length() + mmal_frame->length > buffer_size_) {
            RTC_LOG(LS_ERROR)
                << "Failed to append the frame, buffer size is not enough: "
                << buffer->length() + mmal_frame->length
                << ", buffer_size: " << buffer_size_;
            free_list_.push_back(pending_.front());
            pending_.clear();
            return false;
        }
        mmal_buffer_header_mem_lock(mmal_frame);
        buffer->append(mmal_frame);
        mmal_buffer_header_mem_unlock(mmal_frame);
        // FRame Ended, so forward to encoded frame queue
        if (buffer->isFrameEnd()) {
            // remove the buffer from pending container
            pending_.pop_back();
            encoded_frame_queue_.push_back(buffer);
            Set();  // Event set to wake up DrainThread
        }
        return true;
    }

    // new frame
    buffer = free_list_.front();
    buffer->reset();
    free_list_.pop_front();
    mmal_buffer_header_mem_lock(mmal_frame);
    buffer->copy(mmal_frame);
    mmal_buffer_header_mem_unlock(mmal_frame);
    if (buffer->isConfig()) {
        // this is config frame so it need to append the key frame after it
        pending_.push_back(buffer);
        return true;
    }

    encoded_frame_queue_.push_back(buffer);
    Set();  // Event set to wake up DrainThread
    return true;
}

}  // namespace webrtc
