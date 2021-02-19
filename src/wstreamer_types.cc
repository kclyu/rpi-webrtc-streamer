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

#include "wstreamer_types.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_utils.h"

namespace wstreamer {

namespace {
const double kVideoRoiMin = 0.2f;
const double kVideoRoiMax = 1.0f;
}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// VideoResolution
//
////////////////////////////////////////////////////////////////////////////////
bool VideoResolution::FromString(const std::string resolution_str) {
    constexpr char delimiter[] = "x";
    size_t pos = 0;
    std::string token;
    int value;

    if ((pos = resolution_str.find(delimiter)) != std::string::npos) {
        token = resolution_str.substr(0, pos);
        // getting width value
        if (rtc::FromString(token, &value) == false) {
            status_ = INVALID;
            return false;
        }
        width_ = value;
    }

    // getting height value
    token = resolution_str.substr(pos + 1, std::string::npos);
    if (rtc::FromString(token, &value) == false) {
        status_ = INVALID;
        return false;
    }
    height_ = value;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// VideoRoi
//
////////////////////////////////////////////////////////////////////////////////
bool VideoRoi::Set(ACCESS index, double value) {
    switch (index) {
        case X:
            x_ = value;
            if (isless(value, 0.0f) == true ||
                isgreater(value, kVideoRoiMax) == true) {
                RTC_LOG(LS_ERROR) << "Error, out of valid value range";
                return false;
            }
            return true;
        case Y:
            y_ = value;
            if (isless(value, 0.0f) == true ||
                isgreater(value, kVideoRoiMax) == true) {
                RTC_LOG(LS_ERROR) << "Error, out of valid value range";
                return false;
            }
            return true;
        case WIDTH:
            width_ = value;
            if (isgreater(value, kVideoRoiMin) == false) {
                RTC_LOG(LS_ERROR) << "Error, the value of width must be "
                                     "greater than 0.2."
                                  << ", roi width: " << width_;
                return false;
            }
            return true;
        case HEIGHT:
            height_ = value;
            if (isgreater(value, kVideoRoiMin) == false) {
                RTC_LOG(LS_ERROR) << "Error, the value of width must be "
                                     "greater than 0.2."
                                  << ", roi height: " << height_;
                return false;
            }
            return true;
    }
    return false;
}

bool VideoRoi::isValid() const {
    if (isless(x_, 0) == true || isless(y_, 0) == true) {
        RTC_LOG(LS_ERROR) << "Error, the value of x, y must be greater than 0.0"
                          << ", roi x: " << x_ << "roi x: " << y_;
        return false;
    }
    // validate correlation between value
    if (isgreater(x_ + width_, kVideoRoiMax) == true) {
        RTC_LOG(LS_ERROR)
            << "Error, the value of x + width must be less than 1.0."
            << ", roi x: " << x_ << "roi width: " << width_;
        return false;
    }
    if (isgreater(y_ + height_, kVideoRoiMax) == true) {
        RTC_LOG(LS_ERROR)
            << "Error, the value of y + height must be less than 1.0."
            << ", roi y: " << y_ << "roi heigth: " << height_;
        return false;
    }
    return true;
}

};  // namespace wstreamer
