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
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  */

#ifndef APP_CLIENTINFO_H_
#define APP_CLIENTINFO_H_

#include <list>
#include <memory>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/synchronization/mutex.h"

// AppClient does not have room concept and only one clinet connection..
// so room_id and client_id will be held within AppClientInfo
class AppClientInfo {
    enum ClientState {
        CLIENT_DISCONNECTED = 0,
        CLIENT_REGISTERED,
        CLIENT_DISCONNECT_WAIT
    };

   public:
    explicit AppClientInfo();
    ~AppClientInfo(){};

    bool Register(int sockid, int room_id, int client_id);
    bool Disconnect(int sockid);
    bool GetSockId(int client_id, int& sockid);
    bool IsRegistered(int sockid);
    void Reset();

   private:
    webrtc::Mutex mutex_;
    // Status transition
    ClientState state_;
    int client_id_;
    int room_id_;
    int sockid_;
};

#endif  // APP_CLIENTINFO_H_
