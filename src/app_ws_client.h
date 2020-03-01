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
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <deque>
#include <list>
#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "app_clientinfo.h"
#include "compat/optionsfile.h"
#include "config_media.h"
#include "config_streamer.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/strings/json.h"
#include "streamer_observer.h"
#include "utils.h"
#include "websocket_server.h"

#ifndef APP_WS_CLIENT_H_
#define APP_WS_CLIENT_H_

class AppWsClient : public rtc::MessageHandler,
                    public WebSocketHandler,
                    public SocketServerHelper {
   public:
    enum EventType { EventNotice, EventError };

    explicit AppWsClient();
    ~AppWsClient();

    // register WebSocket Server for SendMessage
    void RegisterWebSocketMessage(WebSocketMessage* server_instance);
    void RegisterConfigStreamer(StreamerConfig* streamer_config);

    // WebSocket Handler
    void OnConnect(int sockid) override;
    bool OnMessage(int sockid, const std::string& message) override;
    void OnDisconnect(int sockid) override;
    void OnError(int sockid, const std::string& message) override;

    // StreamerObserver interface
    bool SendMessageToPeer(const int peer_id,
                           const std::string& message) override;
    void ReportEvent(const int peer_id, bool drop_connection,
                     const std::string& message) override;

   private:
    // message handler interface
    void OnMessage(rtc::Message* msg);

    void SendResponseDeviceId(int sockid, bool success,
                              const std::string& deviceid);
    void SendResponse(int sockid, bool success, const std::string& type,
                      const std::string& data, const std::string& error_mesg);
    void SendEvent(int sockid, EventType is_event,
                   const std::string& event_mesg);

    std::string ws_url_;
    AppClientInfo app_client_;
    WebSocketMessage* websocket_message_;

    // WebSocket chunked frames
    std::string chunked_frames_;
    int num_chunked_frames_;
    std::string deviceid_;
    bool deviceid_inited_;
    ConfigMedia* config_media_;
    StreamerConfig* streamer_config_;
};

#endif  // APP_WS_CLIENT_H_
