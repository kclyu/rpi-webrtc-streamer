/*
Copyright (c) 2017, rpi-webrtc-streamer Lyu,KeunChang

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

#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#ifdef __STANDALONE__
#include <glog/logging.h>

#include <type_traits>
#define RTC_DCHECK CHECK
#define RTC_ERROR LOG(ERROR)
#else
#include "common_types.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#define RTC_ERROR LOG(LS_ERROR)
#endif

#include "raspi_motionvector.h"

// Inline Motion Vector Debugging Macros
// #define DEBUG_IMV

#ifdef DEBUG_IMV
#define DEBUG_IMV_FORMAT(format, args...) printf(format, args);
#define DEBUG_IMV_LOG(msg) printf("%s", msg);
#define DEBUG_IMV_DO(a) a
#else
#define DEBUG_IMV_FORMAT(format, args...)
#define DEBUG_IMV_LOG(msg)
#define DEBUG_IMV_DO(a)
#endif  //  DEBUG_IMV

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// Motion Vector Default Parameters
static const int kMvPixelWidth = 16;

// Bit Operation
static const int kMotionBitSetNumber = 31;
static const int kMotionCutBitThreshold = 2;

// Default Motion Active Max/Min Treshold
static const int kDefaultMotionTriggerPercent = 20;
static const int kDefaultMotionClearPercent = 10;
static const int kDefaultMotionPersistentPeriod = 500;  // ms
static const int kDefaultMotionCoolingDown = 3000;      // ms

RaspiMotionVector::RaspiMotionVector(int x, int y, int framerate,
                                     bool use_imv_coordination) {
    if (use_imv_coordination) {
        mvx_ = x + 1;
        mvy_ = y;
    } else {
        mvx_ = x / kMvPixelWidth + 1;
        mvy_ = y / kMvPixelWidth;
    }
    valid_mv_frame_size_ = mvx_ * mvy_ * sizeof(MotionVector);
    blob_enable_ = false;
    binary_oper_ = 0b1;
    binary_oper_ <<= kMotionBitSetNumber;

    candidate_ = new uint32_t[mvx_ * mvy_];
    motion_ = new uint8_t[mvx_ * mvy_];
    update_counter_ = 0;
    initial_coolingdown_ = (framerate * kDefaultMotionCoolingDown) / 1000;
    enable_observer_callback_ = false;
    blob_observer_ = nullptr;
    imv_observer_ = nullptr;
    blob_active_count_ = 0;
    blob_active_updates_ = 0;
}

RaspiMotionVector::~RaspiMotionVector() {
    delete candidate_;
    delete motion_;
}

void RaspiMotionVector::RegisterBlobObserver(MotionBlobObserver *observer) {
    blob_observer_ = observer;
}

void RaspiMotionVector::RegisterImvObserver(MotionImvObserver *observer) {
    imv_observer_ = observer;
}

int RaspiMotionVector::BitCount(uint32_t motion_value) {
    static const int S[] = {1, 2, 4, 8, 16};  // Magic Binary Numbers
    static const int B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF,
                            0x0000FFFF};

    uint32_t bit_value = motion_value;
    uint32_t bit_count;

    bit_count = bit_value - ((bit_value >> 1) & B[0]);
    bit_count = ((bit_count >> S[1]) & B[1]) + (bit_count & B[1]);
    bit_count = ((bit_count >> S[2]) + bit_count) & B[2];
    bit_count = ((bit_count >> S[3]) + bit_count) & B[3];
    bit_count = ((bit_count >> S[4]) + bit_count) & B[4];
    return bit_count;
}

bool RaspiMotionVector::Analyse(uint8_t *buffer, int len) {
    RTC_DCHECK(valid_mv_frame_size_ == len) << "Motion Vector size mismatch!";
    MotionVector *imv = (MotionVector *)buffer;
    uint32_t base_my, mx, my;

    // vector max/avg
    double magni_value, magni_avg, magni_sum;

    // motion max/avg
    float motion_max, motion_min, motion_avg, motion_sum;
    uint32_t motion_value;
    int motion_active;

    magni_avg = magni_value = magni_sum = 0;
    motion_max = motion_sum = 0;
    motion_min = std::numeric_limits<uint32_t>::max();

    for (my = 0; my < mvy_; my++) {
        base_my = my * mvx_;
        for (mx = 0; mx < mvx_; mx++) {
            magni_value = std::floor(
                std::sqrt(imv[base_my + mx].mx_ * imv[base_my + mx].mx_ +
                          imv[base_my + mx].my_ * imv[base_my + mx].my_));
            if (magni_value) {
                motion_value = candidate_[base_my + mx] =
                    (candidate_[base_my + mx] >> 1) | binary_oper_;
            } else {
                motion_value = candidate_[base_my + mx] =
                    candidate_[base_my + mx] >> 1;
            }

            motion_max = MAX(motion_max, motion_value);
            motion_min = MIN(motion_min, motion_value);
            motion_sum += motion_value;
            magni_sum += magni_value;
        }
    }
    magni_avg = magni_sum / (mvx_ * mvy_);
    motion_avg = motion_sum / (mvx_ * mvy_);

    /* normalize and make final motion */
    motion_active = 0;
    for (my = 0; my < mvy_; my++) {
        base_my = my * mvx_;
        for (mx = 0; mx < mvx_; mx++) {
            motion_value = candidate_[base_my + mx];
            if (BitCount(motion_value) > kMotionCutBitThreshold) {
                motion_[base_my + mx] =
                    (uint8_t)std::floor((motion_value - motion_min) /
                                        (motion_max - motion_min) * 255);
                if (motion_[base_my + mx] < 3) {
                    // remove motion point less then 3
                    motion_[base_my + mx] = 0;
                } else {
                    motion_active += 1;
                }
            } else
                motion_[base_my + mx] = 0;
        };
    };

    if (enable_observer_callback_ && imv_observer_)
        // Reports the number of active motion point
        imv_observer_->OnActivePoints(mvx_ * mvy_, motion_active);

    DEBUG_IMV_FORMAT("MV motion max: %.1f, avg: %.1f, active point: %d, %d%%\n",
                     motion_max, motion_avg, motion_active,
                     (int)motion_active * 100 / (mvx_ * mvy_));
    DEBUG_IMV_DO(fflush(0));

    update_counter_++;
    if (enable_observer_callback_ == false) {
        // wait for initial coolingdown period
        // At the beginning of video encoding, there are many unstable
        // movements of motion vectors, so early IMVs are not used in
        // motion detection.
        // Sends a notification to the observer after the IMVs are created
        // reliably.
        if (initial_coolingdown_ < update_counter_) {
            enable_observer_callback_ = true;
        }
    }

    if (blob_enable_) {
        blob_->UpdateBlob(motion_, mvx_ * mvy_);
        if (blob_observer_) {
            uint32_t new_blob_active_count = blob_->GetActiveBlobCount();
            // Report the active number of blob
            // only when the counter is different from old one.
            if (enable_observer_callback_ &&
                new_blob_active_count != blob_active_count_) {
                blob_active_count_ = new_blob_active_count;
                if (new_blob_active_count) {
                    // Call Trigger
                    blob_observer_->OnMotionTriggered(new_blob_active_count);
                } else {
                    // Call Clear
                    blob_observer_->OnMotionCleared(blob_active_updates_);
                    // reset the blob update counters
                    blob_active_updates_ = 0;
                }
            };
            blob_active_updates_ = blob_->GetActiveBlobUpdateCount();
        };
    }
    return 0;
}

void RaspiMotionVector::GetMotionImage(uint8_t *buffer, int len) {
    RTC_DCHECK(len >= (mvx_ * mvy_))
        << "Motion buffer size is too small to copy!";
    std::memcpy(buffer, motion_, mvx_ * mvy_);
}

void RaspiMotionVector::GetIMVImage(uint8_t *buffer, int len) {
    uint16_t base_sy, sx, sy;
    uint32_t latest_image_mask = 0b1;
    latest_image_mask <<= kMotionBitSetNumber;
    for (sy = 0; sy < mvy_; sy++) {
        base_sy = sy * mvx_;
        for (sx = 0; sx < mvx_; sx++) {
            if (latest_image_mask & candidate_[base_sy + sx]) {
                buffer[base_sy + sx] = 255;
            } else {
                buffer[base_sy + sx] = 0;
            }
        }
    }
}

bool RaspiMotionVector::GetBlobImage(uint8_t *buffer, int len) {
    RTC_DCHECK(len >= (mvx_ * mvy_))
        << "Blob Image buffer size is too small to copy!";
    if (blob_enable_) {
        blob_->GetBlobImage(buffer, len);
        return true;
    }
    // no managed blob_ object
    return false;
}

void RaspiMotionVector::SetBlobEnable(bool blob_enable) {
    RTC_LOG(INFO) << "Blob Analyse Enable: " << blob_enable;
    if (blob_enable) {
        blob_enable_ = true;
        blob_.reset(new RaspiMotionBlob(mvx_, mvy_));
    } else {
        blob_enable_ = false;
        blob_.reset();
    }
}
