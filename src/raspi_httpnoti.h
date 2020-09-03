/*
Copyright (c) 2018, rpi-webrtc-streamer Lyu,KeunChang

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

#ifndef RASPI_HTTPNOTI_H_
#define RASPI_HTTPNOTI_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "rtc_base/net_helpers.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/signal_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

class RaspiHttpNoti : public sigslot::has_slots<>, public rtc::MessageHandler {
   public:
    enum State {
        HTTPNOTI_INIT_ERROR = 0,
        HTTPNOTI_NOT_INITED,
        HTTPNOTI_ADDR_RESOLVING,
        HTTPNOTI_IFADDR_RESOLVING,
        HTTPNOTI_AVAILABLE,
        HTTPNOTI_IN_USE,
        HTTPNOTI_NOT_AVAILABLE,
    };
    explicit RaspiHttpNoti();
    ~RaspiHttpNoti();

    bool Initialize(const std::string url);
    bool IsInited(void);

    void SendMotionNoti(const std::string motion_file);

    void OnMessage(rtc::Message* msg);

   protected:
    void ActivateSending(const std::string& noti);
    void DeactivateSending(void);

    // Resolve server name
    void OnResolveResult(rtc::AsyncResolverInterface* resolver);

    // Resolve ip address of network interface
    void DoIfAddrResolveConnect(void);
    void OnIfAddrConnect(rtc::AsyncSocket* socket);
    void OnIfAddrResolveClose(rtc::AsyncSocket* socket, int err);

    void DoConnect(void);
    void Close();

    rtc::AsyncSocket* CreateClientSocket(int family);
    void OnRead(rtc::AsyncSocket* socket);
    void OnConnect(rtc::AsyncSocket* socket);
    void OnClose(rtc::AsyncSocket* socket, int err);

    // Returns true if the whole response has been read.
    bool ReadIntoBuffer(rtc::AsyncSocket* socket, std::string& data,
                        size_t* content_length);
    int GetResponseStatus(const std::string& response);
    bool GetHeaderValue(const std::string& data, size_t eoh,
                        const char* header_pattern, size_t* value);
    bool GetHeaderValue(const std::string& data, size_t eoh,
                        const char* header_pattern, std::string* value);

    std::list<std::string> noti_message_list_;
    std::string response_data_;

    std::string server_;
    int port_;
    std::string path_;
    std::string deviceid_;
    std::unique_ptr<rtc::AsyncSocket> http_socket_;
    rtc::SocketAddress server_address_;
    rtc::SocketAddress local_address_;
    State state_;
    rtc::AsyncResolver* resolver_;
    webrtc::Mutex mutex_;
    RTC_DISALLOW_COPY_AND_ASSIGN(RaspiHttpNoti);
};

#endif  // RASPI_HTTPNOTI_H_
