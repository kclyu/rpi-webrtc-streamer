/*
Copyright (c) 2020, rpi-webrtc-streamer Lyu,KeunChang

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

#include "session_config.h"

#include <iostream>
#include <memory>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "utils_pc_config.h"

SessionConfig::SessionConfig() {}
SessionConfig::~SessionConfig() {}

void SessionConfig::SetRtcConfig(int id, std::string str_rtc_config) {
    webrtc::MutexLock lock(&mutex_);
    RTC_LOG(INFO) << __FUNCTION__ << ", id : " << id;
    for (std::vector<Config>::iterator it = session_pc_config_.begin();
         it != session_pc_config_.end(); ++it) {
        // update rtc config
        if (it->id_ == id) {
            it->rtc_config_ = str_rtc_config;
            return;
        }
    }

    RTC_LOG(INFO) << "SessionConfig id " << id
                  << " not found, adding new config ";
    session_pc_config_.push_back(Config(id, str_rtc_config));
}

bool SessionConfig::GetConfig(int id, Config &pc_config) {
    webrtc::MutexLock lock(&mutex_);
    RTC_LOG(INFO) << __FUNCTION__ << ", id : " << id;
    for (std::vector<Config>::iterator it = session_pc_config_.begin();
         it != session_pc_config_.end(); ++it) {
        if (it->id_ == id) {
            RTC_LOG(INFO) << "SessionConfig Get id found: " << it->id_
                          << ", rtc_config : " << !it->rtc_config_.empty();
            pc_config = *it;
            return true;
        }
    }
    RTC_LOG(LS_ERROR) << "SessionRtcConfig id not found : " << id;
    return false;
}

void SessionConfig::Remove(int id) {
    webrtc::MutexLock lock(&mutex_);
    RTC_LOG(INFO) << __FUNCTION__ << ", id: " << id
                  << ", size : " << session_pc_config_.size();

    for (std::vector<Config>::iterator it = session_pc_config_.begin();
         it != session_pc_config_.end(); ++it) {
        if (it->id_ == id) {
            RTC_LOG(INFO) << "SessionRtcConfig Remove found id: " << it->id_;
            session_pc_config_.erase(it);
            RTC_LOG(INFO) << "SessionConfig size : "
                          << session_pc_config_.size();
            return;
        }
    }
    RTC_LOG(INFO) << "SessionConfig id not found : " << id
                  << ", size : " << session_pc_config_.size();
}

