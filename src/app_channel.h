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


#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/json.h"

#include "webrtc/base/fileutils.h"
#include "webrtc/base/optionsfile.h"
#include "webrtc/base/pathutils.h"

#include "websocket_server.h"
#include "streamer_observer.h"
#include "app_client.h"

#ifndef APP_CHANNEL_H_
#define APP_CHANNEL_H_

class AppChannel : public LibWebSocketServer, public WebSocketHandler,
    public HttpHandler, public SocketServerHelper {
public:
    explicit AppChannel(const std::string& app_conf);
    ~AppChannel();

    bool AppInitialize();
    // WebSocket Handler
    void OnConnect(int sockid) override;
    void OnMessage(int sockid, const std::string& message) override;
    void OnDisconnect(int sockid) override;
    void OnError(int sockid, const std::string& message) override;

    // HttpHandler
    bool DoGet(HttpRequest* req, HttpResponse* res) override;
    bool DoPost(HttpRequest* req, HttpResponse* res) override;

    // StreamerObserver interface
    bool SendMessageToPeer(const int peer_id, const std::string &message) override;
    void RegisterObserver(StreamerObserver* callback) override;
private:
    bool LoadConfig();

    std::unique_ptr<rtc::OptionsFile> options_;
    std::string app_conf_;
    std::string web_root_;
    std::string ws_server_;
    std::string post_server_;
    std::string home_path_;
    int port_num_;
    std::map <std::string,std::string> room_params_;
    bool is_inited_;
    AppClientInfo app_client_;

    // WebSocket chunked frames 
    std::string chunked_frames_;
    int num_chunked_frames_;
};

#endif // APP_CHANNEL_H_

