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
#define RTC_ERROR RTC_LOG(LS_ERROR)
#endif

#include <list>

#include "config_motion.h"
#include "raspi_motionblob.h"

#define SOURCE_POINT(x, y) source_[(y)*mvx_ + x]
#define EXTRACTED_POINT(x, y) extracted_[(y)*mvx_ + x]
#define PREVIOUS_POINT(x, y) extracted_previous_[(y)*mvx_ + x]
#define BLOB_POINT(blob_id, x, y) blob_list_[blob_id].blob_[(y)*mvx_ + x]
#define SET_BLOB_POINT(blob_id, x, y, value)               \
    if (blob_list_[blob_id].blob_ == nullptr)              \
        RTC_ERROR << "Accessing nullptr blob_" << blob_id; \
    blob_list_[blob_id].blob_[(y)*mvx_ + x] = value;

////////////////////////////////////////////////////////////////////////////////
//
// Debuging Macros
//
////////////////////////////////////////////////////////////////////////////////

#define DEBUG_BLOB_ENABLE  // enable Blob debug macros

// #define DEBUG_BLOB_POINT_TRACE       // enable Blob Point debug macros
// #define DEBUG_BLOB_DUMP_ENABLE          // enable Blob debug macros
// #define DEBUG_TRACK                     // eanble Blob Tracking debug macros

#ifdef DEBUG_BLOB_ENABLE
#define DEBUG_BLOB_FORMAT(format, args...) printf(format, args);
#define DEBUG_BLOB_LOG(msg) printf("%s", msg);
#define DEBUG_BLOB_DO(a) a
#else
#define DEBUG_BLOB_FORMAT(format, args...)
#define DEBUG_BLOB_LOG(msg)
#define DEBUG_BLOB_DO(a)
#endif  //  DEBUG_BLOB_ENABLE

#ifdef DEBUG_BLOB_DUMP_ENABLE
#define DEBUG_BLOB_DUMP(a) a
#else
#define DEBUG_BLOB_DUMP(a)
#endif  // DEBUG_BLOB_DUMP_ENABLE

// Blob Tracking debug message
#ifdef DEBUG_TRACK
#define DEBUG_TRACK_FORMAT(format, args...) printf(format, args);
#define DEBUG_TRACK_LOG(msg) printf("%s", msg);
#define DEBUG_TRACK_DO(a) a
#else
#define DEBUG_TRACK_FORMAT(format, args...)
#define DEBUG_TRACK_LOG(msg)
#define DEBUG_TRACK_DO(a)
#endif  //  DEBUG_TRACK

////////////////////////////////////////////////////////////////////////////////

static const uint8_t BLOB_ID_NOT_USED = 0;  // not used for blob id
static const uint8_t BLOB_ID_CRUMBS = 1;    // used for bread crumbs
static const uint8_t BLOB_ID_MIN = 2;
static const uint8_t BLOB_ID_MAX = 255;

RaspiMotionBlob::RaspiMotionBlob(int mvx, int mvy) : mvx_(mvx), mvy_(mvy) {
    blob_cancel_threshold_ =
        (mvx_ * mvy_ * config_motion::blob_cancel_threshold) / 100;
    blob_tracking_threshold_ = config_motion::blob_tracking_threshold;
    RTC_LOG(INFO) << "Cancel Treshold : " << blob_cancel_threshold_
                  << ", Track treshold : " << blob_tracking_threshold_
                  << ", IMV size: " << mvx_ * mvy_;
    extracted_ = new uint8_t[mvx_ * mvy_];
    extracted_previous_ = new uint8_t[mvx_ * mvy_];
    source_ = new uint8_t[mvx_ * mvy_];  // source_ is uint8_t
    std::fill(extracted_, extracted_ + mvx_ * mvy_, 0);
}

RaspiMotionBlob::~RaspiMotionBlob() {
    delete extracted_;
    delete extracted_previous_;
    delete source_;
    for (int index = BLOB_ID_MIN; index < MAX_BLOB_LIST_SIZE; index++) {
        if (blob_list_[index].blob_ != nullptr) {
            delete blob_list_[index].blob_;
        }
    }
}

bool RaspiMotionBlob::UpdateBlob(uint8_t *motion, size_t len) {
    RTC_DCHECK(len == (mvx_ * mvy_)) << "Motion size does not match!";
    uint32_t sx, sy, base_sy;
    int blob_id = 0;
    size_t blob_size;
    std::memcpy(extracted_previous_, extracted_, len);
    std::fill(extracted_, extracted_ + len, 0);
    std::memcpy(source_, motion, len);  //  source/motion type is uint8_t

    // reset the blob list
    active_blob_list_.clear();

    DEBUG_TRACK_LOG("Starting Blob Detection\n");
    for (sy = 0; sy < mvy_; sy++) {
        base_sy = sy * mvx_;
        for (sx = 0; sx < mvx_; sx++) {
            // found the first blob starting point
            if (source_[base_sy + sx] > 0) {
                // check whether blob_id is available
                if (AccquireBlobId(&blob_id) == true) {
                    // blob_id is available
                    blob_size = SearchConnectedBlob(sx, sy, blob_id);
                    if (blob_size < blob_cancel_threshold_) {
                        UnaccquireBlobId(blob_id);
                    } else {
                        active_blob_list_.push_back(blob_id);
                        blob_list_[blob_id].status_ = MotionBlob::ACTIVE;
                        blob_list_[blob_id].size_ = blob_size;
                        blob_list_[blob_id].update_counter_ = 1;
                    }
                }
            };
        }
    }

    // Work on combining each blob into a single buffer.
    MergeActiveBlob(active_blob_list_);
    DEBUG_TRACK_LOG("Blob Detection Finished and Tracking Active Blob\n");
    DEBUG_BLOB_DUMP(DumpBlobBuffer("Active Blobs(extracted_)", extracted_););

    // Create the tracking information by combining the information
    // of the overlapping blobs in the previous blob buffer and
    // the newly created blob buffer.
    TrackingBlob(active_blob_list_);

    return true;
}

void RaspiMotionBlob::GetBlobImage(uint8_t *buffer, size_t len) {
    RTC_DCHECK(len >= (mvx_ * mvy_))
        << "Blob Image buffer size is too small to copy!";
    uint32_t ex, ey, base_ey;
    int blob_id;
    for (ey = 0; ey < mvy_; ey++) {
        base_ey = ey * mvx_;
        for (ex = 0; ex < mvx_; ex++) {
            blob_id = extracted_[base_ey + ex];
            // Only blobs with an update count greater than
            // the specified blob_tracking_threshold_ are considered blob
            // images.
            if (blob_id >= BLOB_ID_MIN && blob_list_[blob_id].update_counter_ >
                                              (int)blob_tracking_threshold_) {
                buffer[base_ey + ex] = 255;
            } else {
                buffer[base_ey + ex] = 0;
            }
        }
    }
}

int RaspiMotionBlob::GetActiveBlobCount(void) {
    int active_count = 0;
    for (std::list<int>::iterator it = active_blob_list_.begin();
         it != active_blob_list_.end(); ++it) {
        if (blob_list_[*it].update_counter_ > (int)blob_tracking_threshold_)
            active_count++;
    };
    return active_count;
}

int RaspiMotionBlob::GetActiveBlobUpdateCount(void) {
    int max_update_count = 0;
    for (std::list<int>::iterator it = active_blob_list_.begin();
         it != active_blob_list_.end(); ++it) {
        max_update_count =
            std::max(blob_list_[*it].update_counter_, max_update_count);
    };
    return max_update_count;
}

void RaspiMotionBlob::MergeActiveBlob(std::list<int> &active_blob_list) {
    uint16_t ex, ey, base_ey;
    int blob_id;
    size_t blob_size;

    for (std::list<int>::iterator it = active_blob_list.begin();
         it != active_blob_list.end(); ++it) {
        blob_id = *it;
        blob_size = 0;
        if (blob_list_[blob_id].status_ == MotionBlob::ACTIVE) {
            for (ey = blob_list_[blob_id].sy_;
                 blob_size != (size_t)blob_list_[blob_id].size_ && ey < mvy_;
                 ey++) {
                base_ey = ey * mvx_;
                for (ex = 0; ex < mvx_; ex++) {
                    if (blob_list_[blob_id].blob_[base_ey + ex]) {
                        extracted_[base_ey + ex] =
                            blob_list_[blob_id].blob_[base_ey + ex];
                        blob_size++;
                    }
                }
            };
        } else {
            RTC_ERROR << "Internal Error, Motion Blob id is not active: "
                      << blob_id;
        }
    }
}

size_t RaspiMotionBlob::SearchConnectedBlob(uint8_t x, uint8_t y, int blob_id) {
    bool continue_loop = true;
    BlobPoint blobpoint(0, 0);
    size_t blob_size = 0;
    std::list<BlobPoint> connected_list;
    RTC_DCHECK(blob_list_[blob_id].status_ == MotionBlob::COLLECTING);

    // x,y is starting point
    blobpoint.x_ = blob_list_[blob_id].sx_ = x;
    blobpoint.y_ = blob_list_[blob_id].sy_ = y;

    do {
        blob_size += 1;
        SOURCE_POINT(blobpoint.x_, blobpoint.y_) = 0;  // clear the source
        SET_BLOB_POINT(blob_id, blobpoint.x_, blobpoint.y_, blob_id);
        SearchConnectedNeighbor(blobpoint.x_, blobpoint.y_, blob_id,
                                connected_list);

#ifdef DEBUG_BLOB_POINT_TRACE
        DEBUG_BLOB_FORMAT("*** BP(%d,%d)\n", (int)blobpoint.x_,
                          (int)blobpoint.y_);
        DEBUG_BLOB_DO(fflush(0););
        DEBUG_BLOB_DUMP(DumpList(connected_list););
        DEBUG_BLOB_DUMP(DumpBlobBuffer("blob source", source_););
#endif
        if (connected_list.size() > 0) {
            blobpoint = connected_list.front();
            connected_list.pop_front();
        } else
            continue_loop = false;
    } while (continue_loop);

    return blob_size;
}

bool RaspiMotionBlob::SearchConnectedNeighbor(
    uint8_t x, uint8_t y, int blob_id, std::list<BlobPoint> &connected_list) {
    BlobPoint blobpoint(0, 0);
    bool neighbor_exist = false;

#define NEXT_BLOBPOINT(nx, ny, eval, desc)                                   \
    if (eval && SOURCE_POINT(nx, ny) > 0 &&                                  \
        BLOB_POINT(blob_id, nx, ny) < BLOB_ID_CRUMBS) {                      \
        blobpoint.x_ = nx;                                                   \
        blobpoint.y_ = ny;                                                   \
        SET_BLOB_POINT(blob_id, blobpoint.x_, blobpoint.y_, BLOB_ID_CRUMBS); \
        connected_list.push_back(blobpoint);                                 \
        neighbor_exist = true;                                               \
    };

    // Use '+' filter to perform blob detection.
    NEXT_BLOBPOINT(x + 1, y, x + 1 < (int)mvx_, "R");
    NEXT_BLOBPOINT(x, y + 1, y + 1 < (int)mvy_, "D");
    NEXT_BLOBPOINT(x - 1, y, x - 1 >= 0, "L");
    NEXT_BLOBPOINT(x, y - 1, y - 1 >= 0, "U");

    return neighbor_exist;
}

void RaspiMotionBlob::TrackingBlob(std::list<int> &active_blob_list) {
    uint16_t ex, ey, base_ey;
    int active_bid, previous_bid;
    std::list<int> previous_blob_list;

#ifdef DEBUG_TRACK
    DumpBlobIdList("After Updateblob");
#endif  // DEBUG_TRACK

    for (ey = 0; ey < mvy_; ey++) {
        base_ey = ey * mvx_;
        for (ex = 0; ex < mvx_; ex++) {
            active_bid = extracted_[base_ey + ex];
            previous_bid = extracted_previous_[base_ey + ex];
            // apppend previous blob_id for removing it in active list
            if (previous_bid &&
                std::find(previous_blob_list.begin(), previous_blob_list.end(),
                          previous_bid) == previous_blob_list.end()) {
                // blobid is not found in container, so append it
                previous_blob_list.push_back(previous_bid);
            };

            if (active_bid != BLOB_ID_NOT_USED &&
                previous_bid != BLOB_ID_NOT_USED &&
                blob_list_[active_bid].status_ == MotionBlob::ACTIVE &&
                blob_list_[previous_bid].status_ == MotionBlob::ACTIVE) {
#ifdef DEBUG_TRACK
                // apppend blob_id to active
                if (std::find(active_blob_list.begin(), active_blob_list.end(),
                              active_bid) == active_blob_list.end()) {
                    DEBUG_TRACK_FORMAT(
                        "Internal Error, Blob ID %d not in the active blob "
                        "list",
                        active_bid);
                };
#endif  // DEBUG_TRACK

                blob_list_[active_bid].overlap_size_ += 1;
                // merge with previous update_counter
                blob_list_[active_bid].update_counter_ =
                    std::max(blob_list_[active_bid].update_counter_,
                             blob_list_[previous_bid].update_counter_ + 1);
            };
        }
    }

#ifdef DEBUG_TRACK
    int blob_list_len = 0;
    for (int index = BLOB_ID_MIN; index < MAX_BLOB_LIST_SIZE; index++) {
        if (blob_list_[index].status_ != MotionBlob::UNUSED) {
            blob_list_len++;
        }
    }

    DEBUG_TRACK_LOG("Previous Blob list: ");
    for (std::list<int>::iterator it = previous_blob_list.begin();
         it != previous_blob_list.end(); ++it) {
        DEBUG_TRACK_FORMAT("%d,", *it);
    }
    DEBUG_TRACK_LOG("\n");
    fflush(0);
    DEBUG_TRACK_LOG("Active Blob list: ");
    for (std::list<int>::iterator it = active_blob_list.begin();
         it != active_blob_list.end(); ++it) {
        DEBUG_TRACK_FORMAT("%d,", *it);
    }
    DEBUG_TRACK_LOG("\n");
    fflush(0);

    DEBUG_TRACK_FORMAT(
        "Blob list cnt: %d, active blob cnt: %d, previous cnt: %d\n",
        blob_list_len, (int)active_blob_list.size(),
        (int)previous_blob_list.size());
    DEBUG_TRACK_DO(fflush(0));
    DEBUG_TRACK_DO(for (std::list<int>::iterator it = active_blob_list.begin();
                        it != active_blob_list.end(); ++it) {
        DEBUG_TRACK_FORMAT(
            "Blob ID: %d, status: %d, size: %d, overlap: %d, update: %d\n", *it,
            blob_list_[*it].status_, blob_list_[*it].size_,
            blob_list_[*it].overlap_size_, blob_list_[*it].update_counter_);
        fflush(0);
    });

    if (blob_list_len !=
        (previous_blob_list.size() + active_blob_list.size())) {
        DEBUG_TRACK_LOG(
            "Internal Error, Blob list size mismatch between lists!!!");
    }
#endif  // DEBUG_TRACK

    // remove previous blob_id in blob list
    for (std::list<int>::iterator it = previous_blob_list.begin();
         it != previous_blob_list.end(); ++it) {
        DEBUG_TRACK_FORMAT("Clearing previous Blob in list: %d\n", *it);
        UnaccquireBlobId(*it);
    }

#ifdef DEBUG_TRACK
    DumpBlobIdList("Finish Updateblob");
#endif  // DEBUG_TRACK
}

bool RaspiMotionBlob::AccquireBlobId(int *blob_id) {
    // blobid ranges from 2 to 255
    for (int bid = BLOB_ID_MIN; bid < MAX_BLOB_LIST_SIZE; bid++) {
        if (blob_list_[bid].status_ == MotionBlob::UNUSED) {
            // DEBUG_TRACK_FORMAT("New Bid : %d, Status: %d", bid,
            // blob_list_[bid].status_ );
            *blob_id = bid;
            blob_list_[bid].status_ = MotionBlob::COLLECTING;
            blob_list_[bid].size_ = 0;
            blob_list_[bid].update_counter_ = 0;
            blob_list_[bid].overlap_size_ = 0;
            if (blob_list_[bid].blob_ == nullptr) {
                blob_list_[bid].blob_ = new uint8_t[mvx_ * mvy_];
            };
            std::fill(blob_list_[bid].blob_,
                      blob_list_[bid].blob_ + mvx_ * mvy_, 0);
            return true;
        }
    };
    RTC_LOG(INFO) << "Blob id is not available!";
    return false;
}

void RaspiMotionBlob::UnaccquireBlobId(int blob_id) {
    blob_list_[blob_id].status_ = MotionBlob::UNUSED;
}

void RaspiMotionBlob::SetBlobCancelThreshold(int cancel_min) {
    RTC_DCHECK(cancel_min < 100 && cancel_min > 0);  // cancel_min percent
    blob_cancel_threshold_ = mvx_ * mvy_ * (cancel_min / 100);
}

void RaspiMotionBlob::DumpBlobBuffer(const char *header, uint8_t *buffer) {
    uint16_t base_sy, sx, sy;
    printf("Dumping %s ------------------\n", header);
    for (sy = 0; sy < mvy_; sy++) {
        base_sy = sy * mvx_;
        for (sx = 0; sx < mvx_; sx++) {
            switch ((int)buffer[base_sy + sx]) {
                case 0:
                    printf(".");  // cleared
                    break;
                case 1:
                    printf("C");
                    break;
                default:
                    printf("x");  // marked
                    break;
            }
        }
        printf("\n");
    }
    printf(" ------------------\n");
    fflush(0);
}

void RaspiMotionBlob::DumpList(std::list<BlobPoint> &connected_list) {
    DEBUG_BLOB_FORMAT("list %d : ", (int)connected_list.size());
    for (std::list<BlobPoint>::iterator it = connected_list.begin();
         it != connected_list.end(); ++it) {
        DEBUG_BLOB_FORMAT("(%d,%d:%s)", it->x_, it->y_,
                          EXTRACTED_POINT(it->x_, it->y_) ? "" : "X");
    };
    DEBUG_BLOB_LOG("\n");
    fflush(0);
}

void RaspiMotionBlob::DumpBlobIdList(const char *header) {
    DEBUG_TRACK_FORMAT("Dump Blob ID(%s)\n", header);
    for (int bid = BLOB_ID_MIN; bid < MAX_BLOB_LIST_SIZE; bid++) {
        if (blob_list_[bid].status_ != MotionBlob::UNUSED) {
            DEBUG_TRACK_FORMAT(
                "Blob %d: status(%d),size(%d),update_counter(%d)\n", bid,
                blob_list_[bid].status_, blob_list_[bid].size_,
                blob_list_[bid].update_counter_);
        };
    };
    DEBUG_TRACK_LOG("Dump Done\n");
}
