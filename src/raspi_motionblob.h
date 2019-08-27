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

#ifndef RASPI_MOTIONBLOB_H_
#define RASPI_MOTIONBLOB_H_

#include <list>
#include <memory>

#define MAX_BLOB_LIST_SIZE 256

struct MotionBlob {
    enum MotionBlobStatus {
        UNUSED = 0,
        COLLECTING,
        ACTIVE,
    };
    MotionBlob()
        : sx_(0),
          sy_(0),
          status_(UNUSED),
          size_(0),
          overlap_size_(0),
          update_counter_(0),
          blob_(nullptr){};
    uint8_t sx_, sy_;
    MotionBlobStatus status_;
    int size_;
    int overlap_size_;
    int update_counter_;
    uint8_t bid;
    uint8_t *blob_;
};

struct BlobPoint {
    BlobPoint(uint8_t x, uint8_t y) : x_(x), y_(y){};
    virtual ~BlobPoint() {}
    uint8_t x_;
    uint8_t y_;
};

struct ActiveBlob {
    uint8_t id_;
    int size_;  // percent
    uint32_t frame_update_count_;
};

class RaspiMotionBlob {
   public:
    explicit RaspiMotionBlob(int mvx, int mvy);
    ~RaspiMotionBlob();

    bool UpdateBlob(uint8_t *motion, size_t size);
    void GetBlobImage(uint8_t *buffer, size_t buflen);
    int GetActiveBlobCount(void);
    int GetActiveBlobUpdateCount(void);

    void SetBlobCancelThreshold(int cancel_min);

    // disallow copy and assign
    void operator=(const RaspiMotionBlob &) = delete;
    RaspiMotionBlob(const RaspiMotionBlob &) = delete;

   private:
    bool AccquireBlobId(int *blob_id);
    void UnaccquireBlobId(int blob_id);

    size_t SearchConnectedBlob(uint8_t x, uint8_t y, int blob_id);
    bool SearchConnectedNeighbor(uint8_t x, uint8_t y, int blob_id,
                                 std::list<BlobPoint> &connected_list);

    void MergeActiveBlob(std::list<int> &active_blob_list);
    void TrackingBlob(std::list<int> &active_blob_list);

    void DumpBlobBuffer(const char *header, uint8_t *buffer);
    void DumpList(std::list<BlobPoint> &connected_list);
    void DumpBlobIdList(const char *header);

    uint32_t mvx_, mvy_;
    float blob_cancel_threshold_;
    uint32_t blob_tracking_threshold_;

    uint8_t *extracted_;
    uint8_t *extracted_previous_;
    uint8_t *source_;

    MotionBlob blob_list_[MAX_BLOB_LIST_SIZE];
    std::list<int> active_blob_list_;
};

#endif  // RASPI_MOTIONBLOB_H_
