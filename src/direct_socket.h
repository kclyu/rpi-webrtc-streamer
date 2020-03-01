/*
Copyright (c) 2016, rpi-webrtc-streamer Lyu,KeunChang

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

#ifndef RPI_DIRECT_SOCKET_H_
#define RPI_DIRECT_SOCKET_H_
#pragma once

#include <memory>
#include <string>

#include "rtc_base/net_helpers.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/signal_thread.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "streamer_observer.h"

#define FORCE_CONNECTION_DROP_VALID_DURATION 3000
#define FORCE_CONNECTION_DROP_TRYCOUNT_THRESHOLD 3

#define DIRECTSOCKET_FAKE_PEERID 100
#define DIRECTSOCKET_FAKE_NAME_PREFIX "DS"

class DirectSocketServer : public SocketServerHelper,
                           public rtc::MessageHandler,
                           public sigslot::has_slots<> {
   public:
    DirectSocketServer();
    ~DirectSocketServer();

    bool Listen(const rtc::SocketAddress& address);
    void StopListening(void);

    void RegisterObserver(StreamerObserver* callback) override;
    bool SendMessageToPeer(const int peer_id,
                           const std::string& message) override;
    void ReportEvent(const int peer_id, bool drop_connection,
                     const std::string& message) override;

   private:
    void OnAccept(rtc::AsyncSocket* socket);
    void OnClose(rtc::AsyncSocket* socket, int err);
    void OnRead(rtc::AsyncSocket* socket);

    // message handler interface
    void OnMessage(rtc::Message* msg);

    std::unique_ptr<rtc::AsyncSocket> listener_;
    std::unique_ptr<rtc::AsyncSocket> direct_socket_;

    // local id and name
    int socket_peer_id_;
    std::string socket_peer_name_;
    // read buffer from client
    std::string buffered_read_;

    // Forced connection,
    // release the preempted connection and establish a new attempted
    // connection.
    uint64_t last_reject_time_ms_;
    int connection_reject_count_;
};

#endif  // RPI_DIRECT_SOCKET_H_
