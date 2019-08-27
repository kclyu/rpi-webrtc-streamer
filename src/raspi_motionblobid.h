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

#ifndef RASPI_MOTIONBLOBID_H_
#define RASPI_MOTIONBLOBID_H_

#include <list>
#include <memory>

struct MotionBlob {
    enum MotionBlobStatus {
        UNUSED = 0,
        COLLECTING,
        ACTIVE,
    };
    MotionBlob()
        : status_(UNUSED),
          size_(0),
          sx_(0),
          sy_(0),
          overlap_size_(0),
          update_counter_(0){};
    MotionBlobStatus status_;
    int size_;
    int overlap_size_;
    int update_counter_;
    uint8_t sx_, sy_;
};

class MotionBlobId {
   public:
    explicit MotionBlobId();
    ~RaspiMotionBlobId();

   private:
    bool CreateBlobId(uint16_t* blob_id);
    void FreeBlobId(uint16_t blob_id);
    MotionBlob* GetMotionBlob(uint16_t blob_id);
};

#endif  // RASPI_MOTIONBLOBID_H_
