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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(WEBRTC_POSIX)
#include <unistd.h>
#endif
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/socket.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/stringencode.h"

#include "utils.h"

#include "streamer_observer.h"


////////////////////////////////////////////////////////////////////////////////
// StreamerBridge
////////////////////////////////////////////////////////////////////////////////

StreamerBridge* StreamerBridge::stream_bridge_ = nullptr;
StreamerBridge *StreamerBridge::GetInstance() {
    if(stream_bridge_ == nullptr) {
        stream_bridge_ = new StreamerBridge();
        stream_bridge_->active_peer_id_ = 0;
        stream_bridge_->streamer_callback_ = nullptr;
        stream_bridge_->active_socket_observer_  = nullptr;
    }
    return stream_bridge_;
}

////////////////////////////////////////////////////////////////////////////////
// StreamerBridge Streamer Socket Server Observer bridging
////////////////////////////////////////////////////////////////////////////////

void StreamerBridge::RegisterObserver(StreamerObserver* callback) {
    LOG(INFO) << __FUNCTION__;
    streamer_callback_ = callback;
}

bool StreamerBridge::SendMessageToPeer(const int peer_id, const std::string &message) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(streamer_callback_ != nullptr);
    RTC_DCHECK(active_socket_observer_ != nullptr);
    active_socket_observer_->SendMessageToPeer(peer_id, message);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// StreamerBridge Streamer Observer bridging
////////////////////////////////////////////////////////////////////////////////

bool StreamerBridge::ObtainStreamer(SocketServerObserver *socket_server, 
        int peer_id, const std::string& name ) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(streamer_callback_ != nullptr);
    if( active_socket_observer_ != nullptr ) {
        LOG(INFO) << "Streamer already occupied by another socket server";
        return false;
    }
    else {
        active_peer_id_ = peer_id;
        active_socket_observer_ = socket_server;
        streamer_callback_->OnPeerConnected(peer_id, name );
        return true;
    }
}

void StreamerBridge::ReleaseStreamer(SocketServerObserver *socket_server, int peer_id ) {
    LOG(INFO) << __FUNCTION__;
    if( active_socket_observer_ && active_socket_observer_ == socket_server) {
        active_socket_observer_ = nullptr;
        active_peer_id_ = 0;
        streamer_callback_->OnPeerDisconnected(peer_id);
    }
}

void StreamerBridge::MessageFromPeer( int peer_id, const std::string& message ) {
    LOG(INFO) << __FUNCTION__;
    if( active_socket_observer_ != nullptr ) {
        streamer_callback_->OnMessageFromPeer(peer_id, message );
    }
}

void StreamerBridge::MessageSent(int err) {
    LOG(INFO) << __FUNCTION__;
}

