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

#ifndef RPI_STREAMER_OBSERVER_
#define RPI_STREAMER_OBSERVER_
#pragma once

#include <memory>

#include "raspi_motion.h"
#ifdef __NOTI_ENABLE__
#include "raspi_httpnoti.h"
#endif /* __NOTI_ENABLE__ */

struct StreamerObserver {
    virtual void OnPeerConnected(int peer_id, const std::string& name) = 0;
    virtual void OnPeerDisconnected(int peer_id) = 0;
    virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;
    virtual void OnMessageSent(int err) = 0;

   protected:
    virtual ~StreamerObserver() {}
};

struct SocketServerObserver {
    virtual void RegisterObserver(StreamerObserver* callback) = 0;
    virtual bool SendMessageToPeer(const int peer_id,
                                   const std::string& message) = 0;
    virtual void ReportEvent(const int peer_id, bool drop_connection,
                             const std::string& message) = 0;

   protected:
    virtual ~SocketServerObserver() {}
};

class StreamerProxy;  // forward declaration
class RaspiMotion;    // forward declaration
class SocketServerHelper : public SocketServerObserver {
   public:
    SocketServerHelper();
    void RegisterObserver(StreamerObserver* callback){};

   protected:
    virtual ~SocketServerHelper() {}
    // Offer will be generated at server side.
    bool ActivateStreamSession(const int peer_id, const std::string& peer_name);
    // the messsage is offer message from client
    bool ActivateStreamSession(const int peer_id, const std::string& peer_name,
                               const std::string& message);
    void DeactivateStreamSession();
    void MessageFromPeer(const std::string& message);
    int GetActivePeerId();
    bool IsStreamSessionActive();

   private:
    bool streamsession_active_;
    StreamerObserver* streamer_callback_;
    StreamerProxy* streamer_proxy_;
    std::string peer_name_;
    int peer_id_;
};

class StreamerProxy : public SocketServerObserver {
   public:
    static StreamerProxy* GetInstance();
    bool ObtainStreamer(SocketServerObserver* socket_server, int peer_id,
                        const std::string& name);
    bool ObtainStreamer(SocketServerObserver* socket_server, int peer_id,
                        const std::string& name, const std::string& message);
    void ReleaseStreamer(SocketServerObserver* socket_server, int peer_id);
    void MessageFromPeer(int peer_id, const std::string& message);
    void MessageSent(int err);
    // SocketServerObserver
    void RegisterObserver(StreamerObserver* callback) override;
    bool SendMessageToPeer(const int peer_id,
                           const std::string& message) override;
    void ReportEvent(const int peer_id, bool drop_connection,
                     const std::string& message) override;

#ifdef __NOTI_ENABLE__
    // http noti config information (URL) and enable/disable noti feature
    void UpdateNotiConfig(bool noti_enable, const std::string url);
#endif /* __NOTI_ENABLE__ */

   private:
    StreamerProxy() {}
    StreamerProxy(const StreamerProxy&) {}
    ~StreamerProxy() {}

    static StreamerProxy* streamer_proxy_;
    std::unique_ptr<RaspiMotion> raspi_motion_;
#ifdef __NOTI_ENABLE__
    std::unique_ptr<RaspiHttpNoti> http_noti_;
#endif /* __NOTI_ENABLE__ */
    SocketServerObserver* active_socket_observer_;
    StreamerObserver* streamer_callback_;  // streamer callback
    int active_peer_id_;
    std::string active_peer_name_;
    bool noti_enable_;
    std::string noti_url_;
};

#endif  // RPI_STREAMER_OBSERVER_
