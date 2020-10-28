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

#ifndef SESSION_CONFIG_H_
#define SESSION_CONFIG_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"
#include "utils_pc_config.h"

class SessionConfig {
   public:
    struct Config {
        explicit Config(int id, const std::string &str_rtc_config)
            : id_(id), rtc_config_(str_rtc_config) {}
        Config() : id_(0) {}
        virtual ~Config(){};
        inline const std::string GetRtcConfig(void) const {
            return rtc_config_.empty() == false ? rtc_config_ : "";
        }
        inline void clear() { rtc_config_.clear(); }
        int id_;
        std::string rtc_config_;
		// TODO: need to implement video/audio config
    };

    SessionConfig();
    ~SessionConfig();

    // Temporarily save the rtc config set by the manager through
    // the websocket interface and make it available for use in the session.
    // void Set(int id, utils::RTCConfiguration &pc_config);
    void SetRtcConfig(int id, std::string str_rtc_config);
    bool GetConfig(int id, Config &pc_config);
    void Remove(int id);

   private:
    webrtc::Mutex mutex_;
    // Session RtcConfig
    std::vector<Config> session_pc_config_;
};

#endif  // SESSION_CONFIG_H_
