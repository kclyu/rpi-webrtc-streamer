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

#ifndef RASPI_MOTIONVECTOR_H_
#define RASPI_MOTIONVECTOR_H_

#include <memory>
#include <vector>

#include "raspi_motionblob.h"

struct MotionVector {
    int8_t mx_;
    int8_t my_;
    uint16_t sad;
};

struct MotionBlobObserver {
    virtual void OnMotionTriggered(int active_nums) = 0;
    virtual void OnMotionCleared(int updates) = 0;

   protected:
    virtual ~MotionBlobObserver() {}
};

struct MotionImvObserver {
    virtual void OnActivePoints(int total_points, int active_points) = 0;

   protected:
    virtual ~MotionImvObserver() {}
};

class RaspiMotionVector {
   public:
    explicit RaspiMotionVector(int width, int height, int framerate,
                               bool use_imv_coordination);
    ~RaspiMotionVector();

    bool Analyse(uint8_t *buffer, int len);

    void GetIMVImage(uint8_t *buffer, int buflen);
    void GetMotionImage(uint8_t *buffer, int buflen);
    // available only when blob is enabled
    bool GetBlobImage(uint8_t *buffer, int buflen);

    void SetBlobEnable(bool blob_enable);
    void SetMotionActiveTreshold(int max, int min);
    void RegisterBlobObserver(MotionBlobObserver *observer);
    void RegisterImvObserver(MotionImvObserver *observer);

    // disallow copy and assign
    void operator=(const RaspiMotionVector &) = delete;
    RaspiMotionVector(const RaspiMotionVector &) = delete;

   private:
    // bitcount uses the 'Counting bits set, in parallel' algorithm below.
    // http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
    int BitCount(uint32_t motion_value);

    uint32_t mvx_, mvy_;
    uint32_t valid_mv_frame_size_;
    bool blob_enable_;

    uint32_t binary_oper_;
    // motion vector processing buffers
    uint32_t *candidate_;
    uint8_t *motion_;
    uint64_t update_counter_;
    uint32_t initial_coolingdown_;
    bool enable_observer_callback_;

    std::unique_ptr<RaspiMotionBlob> blob_;
    uint32_t blob_active_count_;
    uint32_t blob_active_updates_;
    MotionBlobObserver *blob_observer_;
    MotionImvObserver *imv_observer_;
    uint32_t motion_trigger_threshold_;
    uint32_t motion_clear_threshold_;
};

#endif  // RASPI_MOTIONVECTOR_H_
