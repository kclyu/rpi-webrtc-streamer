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

#ifndef STREAMER_SIGNALING_H_
#define STREAMER_SIGNALING_H_

#include <memory>

#include "config_motion.h"
#include "raspi_motion.h"
#include "session_config.h"

struct SignalingInbound {
    virtual void OnPeerConnected(int peer_id,
                                 const SessionConfig::Config& config) = 0;
    virtual void OnPeerDisconnected(int peer_id) = 0;
    virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;
    virtual void OnMessageSent(int err) = 0;

   protected:
    virtual ~SignalingInbound() {}
};

struct SignalingOutbound {
    virtual void SetSignalingInbound(SignalingInbound* inbound) = 0;
    virtual bool SendMessageToPeer(const int peer_id,
                                   const std::string& message) = 0;
    virtual void ReportEvent(const int peer_id, bool drop_connection,
                             const std::string& message) = 0;

   protected:
    virtual ~SignalingOutbound() {}
};

class StreamerProxy;  // forward declaration
class RaspiMotion;    // forward declaration
class SignalingChannelHelper : public SignalingOutbound {
   public:
    void SetSignalingInbound(SignalingInbound* inbound){};

   protected:
    SignalingChannelHelper(StreamerProxy* proxy);
    virtual ~SignalingChannelHelper() {}
    // Offer will be generated at server side.
    bool StartSignalingSession(const int peer_id,
                               const SessionConfig::Config& config);
    // the messsage is offer message from client
    bool StartSignalingSession(const int peer_id,
                               const SessionConfig::Config& config,
                               const std::string& message);
    void StopSignalingSession();
    void MessageFromPeer(const std::string& message);
    int GetActivePeerId();
    bool IsSignalingSessionActive();

   private:
    bool streamsession_active_;
    SignalingInbound* signaling_inbound_;
    StreamerProxy* proxy_;
    int peer_id_;
};

class StreamerProxy : public SignalingOutbound {
   public:
    StreamerProxy(RaspiMotionHolder* motion_holder);
    ~StreamerProxy() {}

    bool StartStremerSignaling(SignalingOutbound* outbound, int peer_id,
                               const SessionConfig::Config& config);
    bool StartStremerSignaling(SignalingOutbound* outbound, int peer_id,
                               const SessionConfig::Config& config,
                               const std::string& message);
    void StopStreamerSignaling(SignalingOutbound* outbound, int peer_id);
    void MessageFromPeer(int peer_id, const std::string& message);
    void MessageSent(int err);
    // SignalingOutbound
    void SetSignalingInbound(SignalingInbound* inbound) override;
    bool SendMessageToPeer(const int peer_id,
                           const std::string& message) override;
    void ReportEvent(const int peer_id, bool drop_connection,
                     const std::string& message) override;

   private:
    SignalingOutbound* active_signaling_outbound_;
    SignalingInbound* signaling_inbound_;  // inbound signaling channel
    RaspiMotionHolder* motion_holder_;
    bool motion_enabled_;
    int active_peer_id_;
    std::string active_peer_name_;
};

#endif  // STREAMER_SIGNALING_H_
