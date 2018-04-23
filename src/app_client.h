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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <memory>
#include <string>
#include <list>
#include <deque>


#include "api/mediastreaminterface.h"
#include "api/mediaconstraintsinterface.h"
#include "api/peerconnectioninterface.h"
#include "rtc_base/json.h"

#include "websocket_server.h"
#include "streamer_observer.h"
#include "app_clientinfo.h"

#ifndef APP_CLIENT_H_
#define APP_CLIENT_H_

#define URL_RWS_WS_PREFIX   "/rws/"
#define URL_RWS_WS_POSTFIX  "/app_client"
#define URL_APP_WS          "/rws/app_client"
#define URL_JOIN_CMD        "/join"
#define URL_MESSAGE_CMD     "/message"
#define URL_LEAVE_CMD       "/leave"
#define URL_TURN_SERVER       "/turn_server"

class AppClient : public WebSocketHandler, public HttpHandler,
    public SocketServerHelper {
public:
    explicit AppClient();
    ~AppClient();

    enum State{
        SUCCESS = 0,
        ERROR,
        FULL,
        UNKNOWN_ROOM,
        UNKNOWN_CLIENT,
        DUPLICATE_CLIENT,
        INVALID_REQUEST
    };

    struct Result {
        State state_;
        Json::Value json_params;
    };

    void SetRoomId(std::string& room_id );
    void SetAdditionalWSRule(std::string& rule );
    std::string GetWebSocketURL();

    // register WebSocket Server for SendMessage
    void RegisterWebSocketMessage(WebSocketMessage *server_instance);

    // WebSocket Handler
    void OnConnect(int sockid) override;
    bool OnMessage(int sockid, const std::string& message) override;
    void OnDisconnect(int sockid) override;
    void OnError(int sockid, const std::string& message) override;

    // HttpHandler
    bool DoGet(HttpRequest* req, HttpResponse* res) override;
    bool DoPost(HttpRequest* req, HttpResponse* res) override;

    // StreamerObserver interface
    bool SendMessageToPeer(const int peer_id, const std::string &message) override;
private:
    bool JoinRestCall(HttpRequest* req, HttpResponse* res, Result &result);
    bool MessageRestCall(HttpRequest* req, HttpResponse* res, Result &result);
    bool LeaveRestCall(HttpRequest* req, HttpResponse* res, Result &result);

    WebSocketMessage  *websocket_message_;
    AppClientInfo app_client_;
    bool use_room_id_;
    std::string room_id_;
    std::string websocket_url_;
    std::string additional_ws_rule_;
};

#endif // APP_CLIENT_H_

