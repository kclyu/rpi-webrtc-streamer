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

#include "streamer_observer.h"

#include "config_motion.h"
#include "raspi_motion.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/time_utils.h"
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////
// SocketServerHelper
////////////////////////////////////////////////////////////////////////////////
SocketServerHelper::SocketServerHelper(StreamerProxy* proxy)
    : streamsession_active_(false), proxy_(proxy) {
    streamsession_active_ = false;
}

bool SocketServerHelper::ActivateStreamSession(
    const int peer_id, const SessionConfig::Config& config) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(proxy_ != nullptr);
    RTC_DCHECK(streamsession_active_ != true);

    peer_id_ = peer_id;
    if (proxy_->ObtainStreamer(this, peer_id_, config)) {
        streamsession_active_ = true;
        return true;
    };
    return false;
}

bool SocketServerHelper::ActivateStreamSession(
    const int peer_id, const SessionConfig::Config& config,
    const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(proxy_ != nullptr);
    RTC_DCHECK(streamsession_active_ != true);

    peer_id_ = peer_id;
    if (proxy_->ObtainStreamer(this, peer_id_, config, message)) {
        streamsession_active_ = true;
        return true;
    };
    return false;
}

int SocketServerHelper::GetActivePeerId() {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(proxy_ != nullptr);
    if (streamsession_active_)
        return peer_id_;
    else
        return 0;
}

void SocketServerHelper::MessageFromPeer(const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(proxy_ != nullptr);
    if (streamsession_active_) {
        proxy_->MessageFromPeer(peer_id_, message);
        return;
    };
    RTC_LOG(LS_ERROR)
        << "Stream Session is not active, Failed to pass receive message :"
        << message;
}

void SocketServerHelper::DeactivateStreamSession() {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(proxy_ != nullptr);
    RTC_DCHECK(streamsession_active_);
    streamsession_active_ = false;
    proxy_->ReleaseStreamer(this, peer_id_);
}

bool SocketServerHelper::IsStreamSessionActive() {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(proxy_ != nullptr);
    return streamsession_active_;
}

////////////////////////////////////////////////////////////////////////////////
// StreamerProxy
////////////////////////////////////////////////////////////////////////////////
StreamerProxy::StreamerProxy(ConfigMotion* config_motion)
    : active_socket_observer_(nullptr),
      streamer_callback_(nullptr),
      config_motion_(config_motion),
      active_peer_id_(0) {}

void StreamerProxy::RegisterObserver(StreamerObserver* callback) {
    RTC_LOG(INFO) << __FUNCTION__;
    streamer_callback_ = callback;
}

bool StreamerProxy::SendMessageToPeer(const int peer_id,
                                      const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(streamer_callback_ != nullptr);
    RTC_DCHECK(active_socket_observer_ != nullptr);
    active_socket_observer_->SendMessageToPeer(peer_id, message);
    return true;
}

void StreamerProxy::ReportEvent(const int peer_id, bool drop_connection,
                                const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(streamer_callback_ != nullptr);
    RTC_DCHECK(active_socket_observer_ != nullptr);
    active_socket_observer_->ReportEvent(peer_id, drop_connection, message);
}

////////////////////////////////////////////////////////////////////////////////
// StreamerProxy Streamer Observer bridging
////////////////////////////////////////////////////////////////////////////////

bool StreamerProxy::ObtainStreamer(SocketServerObserver* socket_server,
                                   int peer_id,
                                   const SessionConfig::Config& config) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(streamer_callback_ != nullptr);
    if (active_socket_observer_ != nullptr) {
        RTC_LOG(INFO) << "Streamer already occupied by another socket server";
        return false;
    } else {
        if (config_motion_->GetDetectionEnable() == true) {
            if (raspi_motion_ && raspi_motion_->IsActive()) {
                raspi_motion_->StopCapture();
            }
        }
        active_peer_id_ = peer_id;
        active_socket_observer_ = socket_server;
        streamer_callback_->OnPeerConnected(peer_id, config);
        return true;
    }
}

bool StreamerProxy::ObtainStreamer(SocketServerObserver* socket_server,
                                   int peer_id,
                                   const SessionConfig::Config& config,
                                   const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(streamer_callback_ != nullptr);
    if (active_socket_observer_ != nullptr) {
        RTC_LOG(INFO) << "Streamer already occupied by another socket server";
        return false;
    } else {
        if (config_motion_->GetDetectionEnable() == true) {
            if (raspi_motion_ && raspi_motion_->IsActive()) {
                raspi_motion_->StopCapture();
            }
        }
        active_peer_id_ = peer_id;
        active_socket_observer_ = socket_server;
        streamer_callback_->OnMessageFromPeer(peer_id, message);
        return true;
    }
}

void StreamerProxy::ReleaseStreamer(SocketServerObserver* socket_server,
                                    int peer_id) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (active_socket_observer_ && active_socket_observer_ == socket_server) {
        active_socket_observer_ = nullptr;
        active_peer_id_ = 0;
        streamer_callback_->OnPeerDisconnected(peer_id);

        if (config_motion_->GetDetectionEnable() == true) {
            if (raspi_motion_ && raspi_motion_->IsActive() == false) {
                raspi_motion_.reset(new RaspiMotion(config_motion_));
                //  TODO
                // setting HttpNoti callback
#ifdef __NOTI_ENABLE__
                if (noti_enable_) raspi_motion_->SetHttpNoti(http_noti_.get());
#endif /* __NOTI_ENABLE__ */
                raspi_motion_->StartCapture();
            }
        }
    }
}

// TODO:
#ifdef __NOTI_ENABLE__
// Temporarily implement the relevant part of the HttpNoti object in the Proxy
// class. It need to move that http noti part to manage all HttpNoti in the
// Motion class.
void StreamerProxy::UpdateNotiConfig(bool noti_enable, const std::string url) {
    RTC_LOG(INFO) << __FUNCTION__;
    // make sure that the current value is different from the new value.
    if (noti_enable_ == noti_enable && noti_url_ == url) {
        RTC_LOG(INFO) << "The Http Noti Config value has not changed."
                      << "Enable : " << noti_enable_ << ", URL: " << noti_url_;
        return;
    }
    noti_enable_ = noti_enable;
    noti_url_ = url;

    RTC_LOG(INFO) << "Applying updated Http Noti Config "
                  << "Enable : " << noti_enable_ << ", URL: " << noti_url_;
    if (noti_enable) {
        http_noti_.reset(new RaspiHttpNoti());
        http_noti_->Initialize(url);
        raspi_motion_->SetHttpNoti(http_noti_.get());
        if (raspi_motion_->IsActive() == true) {
            // pass http noti
            raspi_motion_->SetHttpNoti(http_noti_.get());
        }
        return;
    }
    http_noti_.reset();
    raspi_motion_->SetHttpNoti(nullptr);
}
#endif /* __NOTI_ENABLE__ */

void StreamerProxy::MessageFromPeer(int peer_id, const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (active_socket_observer_ != nullptr) {
        streamer_callback_->OnMessageFromPeer(peer_id, message);
    }
}

void StreamerProxy::MessageSent(int err) { RTC_LOG(INFO) << __FUNCTION__; }
