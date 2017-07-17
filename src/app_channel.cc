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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "websocket_server.h"
#include "app_channel.h"
#include "utils.h"


//////////////////////////////////////////////////////////////////////////////////////////
//
// Application Channel
//
//////////////////////////////////////////////////////////////////////////////////////////
AppChannel::AppChannel() : is_inited_(false) {
}

bool AppChannel::AppInitialize(StreamerConfig& config){
    std::string ws_url;
    int port_num;
    std::string web_root;
    std::string additional_ws_rule;

    // LibWebSocket debug log
    if(config.GetLibwebsocketDebugEnable() ) {
        LOG(INFO) << "Enable Websocket Library debug logging mesage";
        LogLevel(LibWebSocketServer::DEBUG_LEVEL_ALL);
    };

    config.GetWebSocketPort(port_num);
    LOG(INFO) << "WebSocket port num : " << port_num;
    if( Init(port_num) == false ) return false;

    config.GetWebRootPath(web_root);
    LOG(INFO) << "Using File Mapping : " << web_root;
    AddFileMapping("/", MAPPING_DEFAULT, web_root );

    config.GetRwsWsURL(ws_url);
    LOG(INFO) << "Using RWS WS client url : " << ws_url;
    ws_client_.RegisterWebSocketMessage(this);
    AddWebSocketHandler(ws_url, SINGLE_INSTANCE, &ws_client_);

    // RoomId 
    app_client_.RegisterWebSocketMessage(this);
    if( config.GetRoomIdEnable() ) {
        std::string room_id;
        config.GetRoomId(room_id);
        app_client_.SetRoomId(room_id);
    };
    // AdditionalWSRule 
    config.GetAdditionalWSRule(additional_ws_rule);
    app_client_.SetAdditionalWSRule(additional_ws_rule);

    LOG(INFO) << "Using App client url : " << app_client_.GetWebSocketURL();
    AddWebSocketHandler(app_client_.GetWebSocketURL(), SINGLE_INSTANCE, &app_client_);
    AddHttpHandler(URL_JOIN_CMD, &app_client_);
    AddHttpHandler(URL_MESSAGE_CMD, &app_client_);
    AddHttpHandler(URL_LEAVE_CMD, &app_client_);

    is_inited_ = true;
    return true;
}

AppChannel::~AppChannel() {
}

