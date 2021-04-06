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

#ifndef COMMON_TYPES_H_
#define COMMON_TYPES_H_

#include <math.h>

#include <list>
#include <memory>

#include "absl/strings/str_format.h"
#include "absl/types/optional.h"

// The config part for global media config is required.
namespace wstreamer {

struct VideoResolution {
    enum Status { INVALID = 0, VALID };
    explicit VideoResolution(int width, int height)
        : width_(width), height_(height), status_(VALID){};
    explicit VideoResolution() : width_(0), height_(0), status_(INVALID){};
    virtual ~VideoResolution(){};
    inline std::string ToString() {
        return absl::StrFormat("%dx%d", width_, height_);
    }
    bool FromString(const std::string resolution_str);
    inline bool isValid() { return status_ == VALID; }
    inline void Set(int width, int height) {
        width_ = width;
        height_ = height;
        status_ = VALID;
    }
    inline bool operator==(const VideoResolution& other) const {
        return width_ == other.width_ && height_ == other.height_;
    }
    inline bool operator!=(const VideoResolution& other) const {
        return !((*this) == other);
    }

    int width_, height_;

   private:
    Status status_;
};

struct VideoRoi {
    enum ACCESS { X = 0, Y, WIDTH, HEIGHT };
    explicit VideoRoi(double x, double y, double width, double height)
        : x_(x), y_(y), width_(width), height_(height) {}
    explicit VideoRoi() : x_(0), y_(0), width_(0), height_(0) {}
    virtual ~VideoRoi(){};
    bool isValid() const;
    inline std::string ToSring() const {
        return absl::StrFormat("Roi (%f,%f,%f,%f)", x_, y_, width_, height_);
    }

    bool Set(ACCESS acc, double value);
    double x_, y_, width_, height_;
};

struct VideoEncodingParams {
    explicit VideoEncodingParams() : VideoEncodingParams(0, 0, 0, 0) {}
    explicit VideoEncodingParams(int width, int height)
        : VideoEncodingParams(width, height, 0, 0) {}
    explicit VideoEncodingParams(int width, int height, int framerate,
                                 int bitrate)
        : width_(width),
          height_(height),
          framerate_(framerate),
          bitrate_(bitrate){};
    inline std::string ToString() {
        return absl::StrFormat("%dx%d@%d, %dkbps", width_, height_, framerate_,
                               bitrate_);
    }
    inline bool IsSameResolution(const VideoEncodingParams& other) {
        return width_ == other.width_ && height_ == other.height_;
    }
    int width_, height_, framerate_, bitrate_;
};

// SetEncoderConfigParams sets the parameters necessary for Video Encoding,
// and the following structure is used for required settings additionally.
struct EncoderSettings {
    absl::optional<bool> imv_enable;
    absl::optional<int> intra_period;
    absl::optional<bool> annotation_enable;
    absl::optional<std::string> annotation_text;
    absl::optional<int> annotation_text_size;
};

struct ZoomOptions {
    enum CMD { IS_ACTIVE = 1, IN, OUT, MOVE, RESET };

    CMD cmd;
    absl::optional<double> center_x;
    absl::optional<double> center_y;
};

struct StillOptions {
    absl::optional<int> width;
    absl::optional<int> height;
    absl::optional<int> cameraNum;
    absl::optional<std::string> filename;
    absl::optional<int> quality;
    absl::optional<int> timeout;
    absl::optional<std::string> extension;
    absl::optional<bool> verbose;
    absl::optional<bool> force_capture;
};

};  // namespace wstreamer

#endif  // COMMON_TYPES_H_
