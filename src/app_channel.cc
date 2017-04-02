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
// Constant Keywords for configuration
//
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//
// Http RoomParams Handler
//
// Param Result Example
//
// {"is_initiator": "true", 
// "room_link"    : "https://appr.tc/r/123456723", 
// "turn_server_override": [], 
// "ice_server_transports": "", 
// "media_constraints": "{\"audio\": true, \"video\": true}", 
// "include_loopback_js": "", 
// "turn_url": "https://computeengineondemand.appspot.com/turn?username=81603595&key=4080218913", 
// "wss_url": "wss://apprtc-ws.webrtc.org:443/ws",     
// "pc_constraints": "{\"optional\": []}", 
// "pc_config": "{\"rtcpMuxPolicy\": \"require\", \"bundlePolicy\":\"max-bundle\", \"iceServers\": []}", 
// "wss_post_url": "https://apprtc-ws.webrtc.org:443", 
// "ice_server_url"    : "https://networktraversal.googleapis.com/v1alpha/iceconfig?key=AIzaSyAJdh2HkajseEIltlZ3SIXO02Tze9sO3NY",     
// "warning_messages": [], 
// "room_id": "123456723", 
// "include_rtstats_js": "<script src=\"/js/rtstats.js\"></script><script src=\"/pako/pako.min.js\"></script>", 
// "version_info": 
//              "{\"gitHash\": \"b85a94fc34c98523de194b6b682a738bb4b02de2\", 
//              \"branch\": \"master\", 
//              \"time\": \"Tue Feb 14 16:10:04 2017 +0100\"}", 
// "error_messages": [], 
// "client_id": "81603595", 
// "bypass_join_confirmation": "false", 
// "is_loopback": "false", 
// "offer_options": "{}", 
// "messages": [], 
// "callstats_params": "{\"appSecret\": \"Ew03QUKBLp5D:hdEwlXtR0IwwOi8nhfAK8zLfKZjPBBYWCvz72h/P908=\",// \"appId\": \"110765241\"}"}, 
// "result": "SUCCESS"}
//
//////////////////////////////////////////////////////////////////////////////////////////
// WebSocket Config
const char kWebRoot[] = "web_root";
const char kWebSocketPort[] = "websocket_port";
const char kLibraryDebug[] = "libwebsocket_debug";
const char kWsServer[] = "ws_server";
const char kPostServer[] = "post_server";

// Room Parameter Config
const char kIsInitiiator[] = "is_initiator";
const char kRoomLink[] = "room_link";
const char kTurnServerOverride[] = "turn_server_override";
const char kIceServerTransports[] = "ice_server_transports";
const char kMediaConstraints[] = "media_constraints";
const char kIncludeLoopbackJs[] = "include_loopback_js";
const char kTurnUrl[] = "turn_url";
const char kWssUrl[] = "wss_url";
const char kPcConstraints[] = "pc_constraints";
const char kPcConfig[] = "pc_config";
const char kWssPostUrl[] = "wss_post_url";
const char kIceServerUrl[] = "ice_server_url"    ;
const char kWarningMessage[] = "warning_messages";
const char kRoomId[] = "room_id";
const char kIncludeRtstatsJs[] = "include_rtstats_js";
const char kVersionInfo[] = "version_info";
const char kErrorMessage[] = "error_messages";
const char kClientId[] = "client_id";
const char kBypassJoinConfirmation[] = "bypass_join_confirmation";
const char kIsLoopback[] = "is_loopback";
const char kOfferOptions[] = "offer_options";
const char kMessages[] = "messages";
const char kCallstatsParams[] = "callstats_params";

const char kWsServerTemplate[]= "__WS_SERVER__";
const char kPostServerTemplate[] ="__POST_SERVER__";
const char kRoomTemplate[] = "__ROOM__";
const char kClientIdTemplate[] = "__CLIENTID__";
const char kHomeTemplate[] = "__HOME__";


//////////////////////////////////////////////////////////////////////////////////////////
//
// AppRtc WebSocket/HTTP Protocol Examples
//
//  ** Initial Connection Establishment **
//
//  Method: POST
//  URL: https://appr.tc/join/123456723
//      Message: null
//      Response:
//      Room response: {"params": {"is_initiator": "true", 
//          "room_link": "https://appr.tc/r/123456723", "turn_server_override": [], 
//          ...
//          \"appId\": \"110765241\"}"}, "result": "SUCCESS"}
//  C->WSS: {"cmd":"register","roomid":"123456723","clientid":"05242738"}
//
//  ** Message Exchange **
//
//  - Http Post
//  URL: https://appr.tc/message/123456723/05242738
//      Message: {"sdp":"v=0\r\no=- 5551224427190256041 2 IN IP4 
//              ...
//              1024\r\n","type":"offer"}
//      
//  URL: https://appr.tc/message/123456723/05242738
//      Message: {"type":"candidate","label":0,"id":"audio","candidate":"candidate:1672956178 
//              1 udp 2122260223 10.0.0.101 60746 typ host generation 0 ufrag WbhF network-id 
//              3 network-cost 10"}
//  - WebSocket
//  WSS->C: {"msg":"{\"type\":\"candidate\",\"label\":0,\" ... "work-cost 10\"}","error":""}
//  WSS->C: {"msg":"{\"sdp\":\"v=0\\r\\no=- 8977826303674326566 2 IN ...  webrtc-datachannel 
//              1024\\r\\n\" //  ,\"type\":\"answer\"}","error":""}
//
//  ** Connection Termination **
//
//  C->WSS: {"cmd":"send","msg":"{\"type\": \"bye\"}"}
//
//  Http Post 
//  URL: https://appr.tc/leave/123456723/05242738
//      Message: null
//
//  Method: DELETE
//  URL: https://apprtc-ws.webrtc.org:443/123456723/05242738
//      Message: ""
//
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//
// Application Channel
//
//////////////////////////////////////////////////////////////////////////////////////////
AppChannel::AppChannel(const std::string& app_conf ) 
    : LibWebSocketServer(),
    app_conf_(app_conf), is_inited_(false),num_chunked_frames_(0)  {
}

void AppChannel::OnConnect(int sockid) {
    LOG(INFO) << "New WebSocket connnection id : " << sockid;
}

const char kJsonCmd[] = "cmd";
const char kJsonCmdRegister[] = "register";
const char kJsonCmdSend[] = "send";
const char kJsonRegisterRoomId[] = "roomid";
const char kJsonRegisterClientId[] = "clientid";
const char kJsonSendMsg[] = "msg";


void AppChannel::OnMessage(int sockid, const std::string& message) {
    LOG(INFO) << __FUNCTION__ << "(" << sockid << ")";

    Json::Reader json_reader;
    Json::Value json_value;
    std::string cmd;
    bool parsing_successful;

    // There is an issue where Chrome & Firefox WebSocket sends json messages 
    // in multiple chunks, so this is the part to solve. 
    // If JSON parsing fails, it will be kept in chunked_frrames up to 5 times and 
    // try to parse the collected chunked_frames again until the next message succeeds.
    if((parsing_successful = json_reader.parse(message, json_value)) == true){
        std::string cmd;
        rtc::GetStringFromJsonObject(json_value, kJsonCmd, &cmd);
    };

    if ( parsing_successful && !cmd.empty() ) {
        // parsing success and found the cmd keyword...
        LOG(INFO) << "JSON Parsing Success: " << message;
    }
    else {
        static const int max_chunked_frames=5;

        // json_value.clear();
        chunked_frames_.append(message);
        LOG(INFO) << "Chunked Frame (" << num_chunked_frames_ << "), Message : " 
            << chunked_frames_;
        if (!json_reader.parse(chunked_frames_, json_value)) {
            // parsing failed
            if( num_chunked_frames_++ > max_chunked_frames )  {
                LOG(INFO) << "Failed to parse, Dropping Chunked frames: " 
                    << chunked_frames_;
                num_chunked_frames_ = 0;
                chunked_frames_.clear();
            }
            return;
        }

        rtc::GetStringFromJsonObject(json_value, kJsonCmd, &cmd);
        if( cmd.empty() ) {
            return;
        }
        // parsing success and cmd keyword found.
        LOG(INFO) << "Chunked frames successful: " << chunked_frames_;
        num_chunked_frames_ = 0;
        chunked_frames_.clear();
        // finally successful parsing
    }

    rtc::GetStringFromJsonObject(json_value, kJsonCmd, &cmd);
    if( !cmd.empty() ) {    // found cmd id
        // command register 
        if(cmd.compare(kJsonCmdRegister)== 0) {
            int client_id, room_id;  
            if( !rtc::GetIntFromJsonObject(json_value, kJsonRegisterRoomId, &room_id) ||
                !rtc::GetIntFromJsonObject(json_value, kJsonRegisterClientId, &client_id)) {
                LOG(LS_ERROR) << "Not found clientid/roomid :" << message;
                return;
            }
            LOG(INFO) << "Room ID: " << room_id << ", Client ID: " << client_id;
            if( app_client_.Connected( sockid, room_id, client_id ) == false ) {
                LOG(LS_ERROR) << "Failed to set room_id/client_id :" << message;
                return;
            };

            if ( IsStreamSessionActive() == false ) {
                if( ActivateStreamSession(client_id,int2str(client_id))  == true ) {
                    LOG(INFO) << "New WebSocket Name: " << client_id;
                };
                return;
            }
            // TODO(kclyu) need to send response of room busy error message
            LOG(INFO) << "WebSocket Already exist : " << GetActivePeerId();
            return;
        }
        // command send
        else if(cmd.compare(kJsonCmdSend)== 0) {
            std::string msg;
            rtc::GetStringFromJsonObject(json_value, kJsonSendMsg, &msg);
            if( !msg.empty() ) {
                MessageFromPeer(msg);
                return;
            }
            LOG(LS_ERROR) << "Failed to pass received message :" << message;
        }
        return;
    };
    LOG(LS_ERROR) << "Received unknown protocol message. " << message;
}


void AppChannel::OnDisconnect(int sockid) {
    LOG(INFO) << "WebSocket connnection id : " << sockid << " closed";
    if ( IsStreamSessionActive() == true ) {
        DeactivateStreamSession();
    };
    app_client_.Reset();
}

void AppChannel::OnError(int sockid, const std::string& message) {
}

bool AppChannel::SendMessageToPeer(const int peer_id, 
        const std::string &message) {
    int websocket_id;

    LOG(INFO) << __FUNCTION__;
    if( app_client_.GetWebSocketId(peer_id, websocket_id ) == true ) {
        Json::StyledWriter json_writer;
        Json::Value json_message;

        json_message[kJsonCmd] = kJsonCmdSend;
        json_message[kJsonSendMsg] = message;
        SendMessage(websocket_id, json_writer.write(json_message));
        return true;
    }
    return false;
}

//  
void AppChannel::RegisterObserver(StreamerObserver* callback) {
    LOG(INFO) << __FUNCTION__;
}

bool AppChannel::AppInitialize(){
    std::string wss_resource;
    size_t pos;

    if( LoadConfig() == false ) return false;
    // Initialize the LibWebSocket server
    if( Init( port_num_ ) == false ) return false;

    // remove server name from WSS URL
    wss_resource = room_params_[kWssUrl];
    if( (pos = wss_resource.find(ws_server_, 0)) 
            != std::string::npos){
        wss_resource.replace(pos,ws_server_.size(),"");
    };

    LOG(INFO) << "Using WSS resouce : " << wss_resource;
    AddWebSocketHandler(wss_resource, SINGLE_INSTANCE, this);
    LOG(INFO) << "Using File Mapping : " << web_root_;
    AddFileMapping("/", MAPPING_DEFAULT, web_root_ );

    is_inited_ = true;
    return true;
}

#define ROOM_PARAM_TEMPLATE_REPLACE(key_id, tpl, replace_string) \
    { size_t pos;    \
        if( (pos = room_params_[key_id].find(tpl, 0))    \
                != std::string::npos){   \
            room_params_[key_id].replace(pos,strlen(tpl),replace_string);    \
        };   \
    }

#define ROOM_PARAM_LOAD(key_id) \
    if(options_->GetStringValue(key_id, &string_value ) == true ) { \
        room_params_[key_id] = string_value; \
        ROOM_PARAM_TEMPLATE_REPLACE(key_id, kWsServerTemplate, ws_server_) \
        ROOM_PARAM_TEMPLATE_REPLACE(key_id, kPostServerTemplate,post_server_) \
        ROOM_PARAM_TEMPLATE_REPLACE(key_id, kHomeTemplate,home_path_) \
        LOG(INFO) << "Room Param ( " << key_id << "): " << room_params_[key_id]; \
    };


bool AppChannel::LoadConfig(){
    std::string string_value;
    int         int_value;
    size_t      pos;

    home_path_ = getenv("HOME");
    options_.reset(new rtc::OptionsFile(app_conf_));

    if( options_->Load() == false ) {
        LOG(LS_ERROR) << "AppChannel initialization failed to load config options:" << app_conf_;
        return false;
    };

    // LibWebsocket debug log flag
    if(options_->GetStringValue(kWebRoot, &string_value ) == false ) {
        LOG(LS_ERROR) << "WebRoot Confg(" << kWebRoot << ") not found.";
        return false;
    }
    LOG(INFO) << "WebRoot : " << string_value ;
    web_root_ = string_value;
    // replace __HOME__ to HOME environment value
    if( (pos = web_root_.find(kHomeTemplate, 0)) 
            != std::string::npos){
        web_root_.replace(pos,strlen(kHomeTemplate),home_path_);
    };

    // LibWebSocket debug log
    string_value.clear();
    if(options_->GetStringValue(kLibraryDebug, &string_value ) == true ) {
        LOG(INFO) << "Websocket Library debug: " << string_value ;
        if( string_value.compare("true") == 0 )
            // change the debug print options
            LogLevel(LibWebSocketServer::DEBUG_LEVEL_ALL);
    }

    // WebSocket Port number
    if(options_->GetIntValue(kWebSocketPort, &int_value ) == false ) {
        LOG(LS_ERROR) << "WebSocket Port Number(" << kWebSocketPort << ") not found.";
        return false;
    }
    LOG(INFO) << "WebSocket Port : " << int_value ;
    port_num_ = int_value;

    // WebSocket Server 
    if(options_->GetStringValue(kWsServer, &string_value ) == false ) {
        LOG(LS_ERROR) << "WebSocket Server (" << kWsServer << ") not found.";
        return false;
    }
    LOG(INFO) << "WebSocket Server : " << string_value ;
    ws_server_ = string_value;

    // Post Server 
    if(options_->GetStringValue(kPostServer, &string_value ) == false ) {
        LOG(LS_ERROR) << "Post Server (" << kPostServer << ") not found.";
        return false;
    }
    LOG(INFO) << "Post Server : " << string_value ;
    post_server_ = string_value;

    ROOM_PARAM_LOAD(kIsInitiiator);
    ROOM_PARAM_LOAD(kRoomLink);
    ROOM_PARAM_LOAD(kTurnServerOverride);
    ROOM_PARAM_LOAD(kIceServerTransports);
    ROOM_PARAM_LOAD(kMediaConstraints);
    ROOM_PARAM_LOAD(kIncludeLoopbackJs);
    ROOM_PARAM_LOAD(kTurnUrl);
    ROOM_PARAM_LOAD(kWssUrl);
    ROOM_PARAM_LOAD(kPcConstraints);
    ROOM_PARAM_LOAD(kPcConfig);
    ROOM_PARAM_LOAD(kWssPostUrl);
    ROOM_PARAM_LOAD(kIceServerUrl);
    ROOM_PARAM_LOAD(kWarningMessage);
    // ROOM_PARAM_LOAD(kRoomId);
    ROOM_PARAM_LOAD(kIncludeRtstatsJs);
    ROOM_PARAM_LOAD(kVersionInfo);
    ROOM_PARAM_LOAD(kErrorMessage);
    ROOM_PARAM_LOAD(kClientId);
    ROOM_PARAM_LOAD(kBypassJoinConfirmation);
    ROOM_PARAM_LOAD(kIsLoopback);
    ROOM_PARAM_LOAD(kOfferOptions);
    ROOM_PARAM_LOAD(kMessages);
    ROOM_PARAM_LOAD(kCallstatsParams);

    return true;
}


bool AppChannel::DoGet(HttpRequest* req, HttpResponse* res)  {
#ifdef _0
    Json::StyledWriter writer;
    Json::Value json_params;
    LOG(INFO) << "DoGet Called";
    LoadParams( json_params );
    res->status_ = 200;
    res->mime_ = "application/json";
    res->response_ = writer.write(json_params);
    req->Print();
    res->Print();
#endif
    return true;
}

bool AppChannel::DoPost(HttpRequest* req, HttpResponse* res)  {
#ifdef _0
    Json::StyledWriter writer;
    Json::Value json_params;
    LOG(INFO) << "DoPost Called";
    res->status_ = 200;
    LoadParams( json_params);
    res->mime_ = "application/json";
    res->response_ = writer.write(json_params);
    req->Print();
    res->Print();
#endif
    return true;
}

#ifdef _0
bool HttpRoomParams::LoadParams(Json::Value &params ) {
    params[kIsInitiiator] = "false";
    params[kRoomLink] = "https://appr.tc/r/123456723";
    params[kWssPostUrl] = "https://appr.tc/r/123456723";
    params[kWssUrl] = "https://appr.tc/r/123456723";
    params[kTurnUrl] = "";
    params[kRoomId] = "";
    params[kPcConstraints] = "";
    params[kPcConfig] = "";
    params[kClientId] = "";
    params[kIsLoopback] = "";
    return true;
}
#endif

AppChannel::~AppChannel() {
}

