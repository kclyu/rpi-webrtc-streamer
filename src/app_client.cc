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

#include "rtc_base/stringencode.h"
#include "websocket_server.h"
#include "app_client.h"
#include "utils.h"

#define URL_SPLITED_POS_NONE        0
#define URL_SPLITED_POS_CMD         1
#define URL_SPLITED_POS_ROOMID      2
#define URL_SPLITED_POS_CLIENTID    3

#define URL_CMD_JOIN_SPLITED_NUM        3
#define URL_CMD_MESSAGE_SPLITED_NUM     4
#define URL_CMD_LEAVE_SPLITED_NUM       4

// Adding Simple Json Key Value and INFO loggging
#define JK_SD(param, key_id, value) \
    param[key_id] = value; \
    RTC_LOG(LS_INFO) << "Key: " << key_id << " : " << value;

// Adding Simple Json Key Value
#define JK_S(param, key_id, value ) \
    param[key_id] = value;

#define TPL_REPLACE(source, tpl, replace_value) \
    {\
        size_t pos; \
        if( (pos = source.find(tpl, 0)) != std::string::npos){   \
            source.replace(pos,strlen(tpl),replace_value);    \
        };   \
    }

//////////////////////////////////////////////////////////////////////////////////////////
//
// Http RoomJoin Handler
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

//////////////////////////////////////////////////////////////////////////////////////////
//
// Constant Keywords for Join Response (Room parameter config key)
//
//////////////////////////////////////////////////////////////////////////////////////////
static const char kIsInitiator[] = "is_initiator";
static const char kRoomLink[] = "room_link";
static const char kTurnServerOverride[] = "turn_server_override";
static const char kIceServerTransports[] = "ice_server_transports";
static const char kMediaConstraints[] = "media_constraints";
static const char kIncludeLoopbackJs[] = "include_loopback_js";
static const char kTurnUrl[] = "turn_url";
static const char kWssUrl[] = "wss_url";
static const char kPcConstraints[] = "pc_constraints";
static const char kPcConfig[] = "pc_config";
static const char kWssPostUrl[] = "wss_post_url";
static const char kIceServerUrl[] = "ice_server_url"    ;
static const char kWarningMessage[] = "warning_messages";
static const char kRoomId[] = "room_id";
static const char kIncludeRtstatsJs[] = "include_rtstats_js";
static const char kVersionInfo[] = "version_info";
static const char kErrorMessage[] = "error_messages";
static const char kClientId[] = "client_id";
static const char kBypassJoinConfirmation[] = "bypass_join_confirmation";
static const char kIsLoopback[] = "is_loopback";
static const char kOfferOptions[] = "offer_options";
static const char kMessages[] = "messages";
static const char kCallstatsParams[] = "callstats_params";

static const char kResult[] = "result";
static const char kParams[] = "params";

// Template
static const char kWsServerTemplate[]= "__WS_SERVER__";
static const char kWebSocketURLTemplate[]= "__WS_URL__";
static const char kHttpServerTemplate[] ="__HTTP_SERVER__";
static const char kRoomTemplate[] = "__ROOMID__";
static const char kClientIdTemplate[] = "__CLIENTID__";
static const char kAdditionalWSRuleTemplate[] = "__ADDITIONAL_WS_RULE__";



//////////////////////////////////////////////////////////////////////////////////////////
//
// Constant Keywords for WebSocket register/bye type command
//
//////////////////////////////////////////////////////////////////////////////////////////
static const char kJsonCmd[] = "cmd";
static const char kJsonCmdRegister[] = "register";
static const char kJsonCmdSend[] = "send";
static const char kJsonRegisterRoomId[] = "roomid";
static const char kJsonRegisterClientId[] = "clientid";
static const char kJsonSendMsg[] = "msg";
static const char kJsonSendMsgType[] = "type";
static const char kJsonSendMsgTypeBye[] = "bye";
static const char kSessionDescriptionSdpName[] = "sdp";

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
// AppRTC Interface Channel
//
//////////////////////////////////////////////////////////////////////////////////////////
AppClient::AppClient()
    : websocket_message_(nullptr), use_room_id_(false){
    websocket_url_ = URL_APP_WS;
    additional_ws_rule_ = "";
}

void AppClient::RegisterWebSocketMessage(WebSocketMessage *server_instance) {
    websocket_message_ = server_instance;
}

void AppClient::SetRoomId(std::string& room_id) {
    use_room_id_ = true;
    room_id_ = room_id;
    websocket_url_ = URL_RWS_WS_PREFIX;
    websocket_url_.append(room_id_).append(URL_RWS_WS_POSTFIX);
}

void AppClient::SetAdditionalWSRule(std::string& rule ){
    additional_ws_rule_ = rule;
}

std::string AppClient::GetWebSocketURL() {
    return websocket_url_;
}

void AppClient::OnConnect(int sockid) {
    RTC_LOG(INFO) << "WebSocket connnection id : " << sockid;
}

bool AppClient::OnMessage(int sockid, const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << sockid << ")";
    Json::Reader json_reader;
    Json::Value json_value;
    std::string cmd;

    if(json_reader.parse(message, json_value) == false){
        RTC_LOG(INFO) << __FUNCTION__ << ", Failed to parse message :" << message;
        return true;
    };

    // The websocket connection does not receive the message directly
    // from the peer client, it will receive command type message(register/bye).
    // Websocket will send messages which generated from rws to the client
    rtc::GetStringFromJsonObject(json_value, kJsonCmd, &cmd);
    if( !cmd.empty() ) {    // found cmd id
        // command register
        if(cmd.compare(kJsonCmdRegister)== 0) {
            int client_id, room_id;
            if( !rtc::GetIntFromJsonObject(json_value, kJsonRegisterRoomId, &room_id) ||
                !rtc::GetIntFromJsonObject(json_value, kJsonRegisterClientId, &client_id)) {
                RTC_LOG(LS_ERROR) << "Not found clientid/roomid :" << message;
                return true;
            }

            RTC_LOG(INFO) << "Tyring to register RoomId: " << room_id << ", ClientID: " << client_id;

            //  If one of the three (socketid, room_id, client_id) does not match,
            //  the Websocket connection will not be connected.
            if( app_client_.Connected( sockid, room_id, client_id ) == false ) {
                RTC_LOG(LS_ERROR) << "Failed to register room_id/client_id :" << message;
                if ( IsStreamSessionActive() == true ) {
                    RTC_LOG(INFO) << "Streamer Session already Active. Try to drop connection: "
                        << sockid;
                    app_client_.DisconnectWait(sockid);
                    DeactivateStreamSession();
                };
                return false;
            };

            return true;
        }
        else if(cmd.compare(kJsonCmdSend)== 0) {
            // {"cmd":"send","msg":"{\"type\": \"bye\"}"}
            std::string msg;
            std::string msg_type;
            Json::Value json_msg_value;
            rtc::GetStringFromJsonObject(json_value, kJsonSendMsg, &msg);
            if(json_reader.parse(msg, json_msg_value) == false){
                RTC_LOG(INFO) << __FUNCTION__ << ", Failed to parse message :" << msg;
                return true;
            }
            rtc::GetStringFromJsonObject(json_msg_value, kJsonSendMsgType, &msg_type);
            if(msg_type.compare(kJsonSendMsgTypeBye) == 0) {
                RTC_LOG(INFO) << __FUNCTION__ << "Terminating Websocket connection";
                return false;
            }

        }

    };
    RTC_LOG(LS_ERROR) << "Received unknown protocol message. " << message;
    return true;
}


void AppClient::OnDisconnect(int sockid) {
    RTC_LOG(INFO) << "WebSocket connnection id : " << sockid << " disconnected";
    app_client_.DisconnectWait(sockid);
    if ( IsStreamSessionActive() == true ) {
        DeactivateStreamSession();
    };
}

void AppClient::OnError(int sockid, const std::string& message) {
}

bool AppClient::SendMessageToPeer(const int peer_id, const std::string &message) {
    RTC_DCHECK(websocket_message_ != nullptr ) << "WebSocket Server instance is nullptr";
    int websocket_id;

    RTC_LOG(INFO) << __FUNCTION__;
    if( app_client_.GetWebsocketId(peer_id, websocket_id ) == true ) {
        Json::StyledWriter json_writer;
        Json::Value json_message;

        json_message[kJsonCmd] = kJsonCmdSend;
        json_message[kJsonSendMsg] = message;
        websocket_message_->SendMessage(websocket_id, json_writer.write(json_message));
        return true;
    }
    return false;
}



//////////////////////////////////////////////////////////////////////////////////////////
//
// Rest POST API implementation
//
//////////////////////////////////////////////////////////////////////////////////////////
bool AppClient::JoinRestCall(HttpRequest* req, HttpResponse* res,Result &result ){
    RTC_LOG(INFO) << __FUNCTION__ << ", URL : " << req->url_;
    int room_id, client_id;
    std::vector<std::string> splited;
    std::string string_value;
    Json::Value json_arrry_value(Json::arrayValue);

    // init result state as SUCCESS
    result.state_ = SUCCESS;

    size_t splited_num = rtc::split( req->url_, '/', &splited);
    if( req->type_ != HttpRequestType::HTTP_POST ||
            splited_num != URL_CMD_JOIN_SPLITED_NUM ||
            utils::StringToInt( splited[URL_SPLITED_POS_ROOMID], &room_id )
            == false ) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "Error in parameters : "
            << splited[URL_SPLITED_POS_ROOMID];
        result.state_ = INVALID_REQUEST;
        return false;
    }

    // Validate the room_id value by comparing it with room_id of join request.
    if( use_room_id_ )  {
        if( room_id_.compare(splited[URL_SPLITED_POS_ROOMID])) {
            RTC_LOG(LS_ERROR) << __FUNCTION__ << "Room ID does not match ((config)"
                << room_id_ << " != (request)"
                << splited[URL_SPLITED_POS_ROOMID] << "), Rejecting Join request" ;
            result.state_ = INVALID_REQUEST;
            return false;
        }
    }

    // Changing app client to CONNECT WAIT state
    if( app_client_.ConnectWait(room_id, client_id) == false ){
        result.state_ = FULL;
        return false;
    }

    // IsInitiator
    JK_S( result.json_params,  kIsInitiator,  "true" );
    //  RoomLink
    string_value  = "https://__HTTP_SERVER__/r/__ROOMID__";
    TPL_REPLACE(string_value, kHttpServerTemplate, req->server_ );
    TPL_REPLACE(string_value, kRoomTemplate, utils::IntToString(room_id));
    JK_SD( result.json_params,  kRoomLink,  string_value );
    // TurnServerOverride
    JK_S( result.json_params,  kTurnServerOverride,  json_arrry_value );
    // IceServerTransports
    JK_S( result.json_params,  kIceServerTransports,  "" );
    // MediaConstraints
    JK_S( result.json_params,  kMediaConstraints,
            "{\"audio\": true, \"video\": true}" );
    // IncludeLoopbackJs
    JK_S( result.json_params,  kIncludeLoopbackJs,  "" );
    // TurnUrl -- not used
    // TPL_REPLACE(string_value, kHttpServerTemplate, req->server_ );
    // JK_SD( result.json_params,  kTurnUrl, string_value);
    // WsUrl
    string_value  = "wss://__HTTP_SERVER____ADDITIONAL_WS_RULE____WS_URL__";
    TPL_REPLACE(string_value, kHttpServerTemplate, req->server_ );
    TPL_REPLACE(string_value, kAdditionalWSRuleTemplate,additional_ws_rule_ );
    TPL_REPLACE(string_value, kWebSocketURLTemplate, websocket_url_);
    JK_SD( result.json_params,  kWssUrl,  string_value);
    // PcConstrains
    JK_S( result.json_params,  kPcConstraints,  "{\"optional\": []}");
    // PcConfig -- using default stun server URL
    // TODO change iceServers based on the configuration
    JK_S( result.json_params,  kPcConfig,
            "{\"rtcpMuxPolicy\": \"require\", \"bundlePolicy\":\"max-bundle\", \
\"iceServers\": [ {\"urls\": \"stun:stun.l.google.com:19302\"}]}");     // TODO config
    // WssPostUrl
    string_value  = "https://__HTTP_SERVER__";
    TPL_REPLACE(string_value, kHttpServerTemplate, req->server_ );
    JK_SD( result.json_params,  kWssPostUrl,  string_value);
    // IceServerUrl -- not used
    // string_value  = "https://__HTTP_SERVER__/turn_urls.json";
    // TPL_REPLACE(string_value, kHttpServerTemplate, req->server_ );
    // JK_SD( result.json_params,  kIceServerUrl,  string_value);
    // WarningMessage
    JK_S( result.json_params,  kWarningMessage,  "" );
    // RoomId
    JK_SD( result.json_params,  kRoomId, utils::IntToString(room_id) );
    // IncludeRtsstatsJs
    JK_S( result.json_params,  kIncludeRtstatsJs, "" );
    // VersionInfo
    JK_S( result.json_params,  kVersionInfo, "" );
    // ErrorMessage
    JK_S( result.json_params,  kErrorMessage, "" );
    // ClientId
    JK_SD( result.json_params,  kClientId, utils::IntToString(client_id));
    // BypassJoinConfirmation
    JK_S( result.json_params,  kBypassJoinConfirmation, "false" );
    // IsLoopback
    JK_S( result.json_params,  kIsLoopback, "false" );
    // OfferOptions
    JK_S( result.json_params,  kOfferOptions, "{}" );
    // CallstatsParams
    JK_S( result.json_params,  kCallstatsParams, "" );
    // Messages
    JK_S( result.json_params,  kMessages, "" );

    return true;
}

bool AppClient::MessageRestCall(HttpRequest* req, HttpResponse* res,Result &result ){
    RTC_LOG(INFO) << __FUNCTION__ << ", URL : " << req->url_;
    Json::Reader json_reader;
    Json::Value json_value;
    int room_id, client_id;
    std::vector<std::string> splited;
    std::string cmd;

    size_t splited_num = rtc::split( req->url_, '/', &splited);
    if( req->type_ != HttpRequestType::HTTP_POST ||
            splited_num != URL_CMD_MESSAGE_SPLITED_NUM ||
            (utils::StringToInt( splited[URL_SPLITED_POS_ROOMID], &room_id)
             == false) ||
            (utils::StringToInt( splited[URL_SPLITED_POS_CLIENTID], &client_id )
             == false)) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "Error in parameters : "
            << splited[URL_SPLITED_POS_ROOMID]
            << splited[URL_SPLITED_POS_CLIENTID];
        result.state_ = INVALID_REQUEST;
        return false;
    }
    if( app_client_.IsConnected(room_id, client_id) == false )  {
        RTC_LOG(LS_ERROR) << "Client " << client_id << " trying to  exchange message ";
        result.state_ = ERROR;
        return false;
    }

    RTC_LOG(INFO) << __FUNCTION__ << " Data : " << req->data_;
    if(json_reader.parse(req->data_, json_value) == false){
        RTC_LOG(INFO) << "Failed to JSON parse : " << req->data_;
        return false;
    }

    std::string sdp;
    rtc::GetStringFromJsonObject(json_value, kSessionDescriptionSdpName, &sdp);
    if (!sdp.empty()) {
        // there is sdp in json message
        if ( IsStreamSessionActive() == false ) {
            if( ActivateStreamSession(client_id,
                        utils::IntToString(client_id), req->data_)  == true ) {
            }
            else {
                // failed to activate stream session
                return false;
            }
        }
    }
    else  {
        // Send message to PC
        MessageFromPeer(req->data_);
    }
    return true;
}

bool AppClient::LeaveRestCall(HttpRequest* req, HttpResponse* res,Result &result ){
    RTC_LOG(INFO) << __FUNCTION__ << ", URL : " << req->url_;
    int room_id, client_id;
    std::vector<std::string> splited;
    size_t splited_num = rtc::split( req->url_, '/', &splited);
    if( req->type_ != HttpRequestType::HTTP_POST ||
            splited_num != URL_CMD_LEAVE_SPLITED_NUM ||
            (utils::StringToInt( splited[URL_SPLITED_POS_ROOMID], &room_id)
             == false) ||
            (utils::StringToInt( splited[URL_SPLITED_POS_CLIENTID], &client_id )
             == false)) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "Error in parameters : "
            << splited[URL_SPLITED_POS_ROOMID]
            << splited[URL_SPLITED_POS_CLIENTID];
        result.state_ = INVALID_REQUEST;
        return false;
    }

    if( app_client_.DisconnectWait(room_id, client_id) == false ) {
        result.state_ = ERROR;
        return false;
    }
    if ( IsStreamSessionActive() == true ) {
        app_client_.DisconnectWait(room_id, client_id);
        DeactivateStreamSession();
    };

    return true;
}

bool AppClient::DoPost(HttpRequest* req, HttpResponse* res)  {
    Json::StyledWriter writer;
    Json::Value json_result;
    Result result;

    if( req->url_.compare(0, sizeof(URL_JOIN_CMD) - 1, URL_JOIN_CMD) == 0 ) {
        if( JoinRestCall( req, res, result) == false ) {
            JK_S( json_result,  kParams, "");
            JK_S( json_result,  kResult, "ERROR" );
            res->status_ = 400;  //
            res->mime_ = "application/json";
            res->response_ = "";
            return true;
        }
        else {
            // Result
            switch (result.state_ ) {
                case SUCCESS:
                    JK_S( json_result,  kResult, "SUCCESS" );
                    break;
                case ERROR:
                    JK_S( json_result,  kResult, "ERROR" );
                    break;
                case FULL:
                    JK_S( json_result,  kResult, "FULL" );
                    break;
                case UNKNOWN_ROOM:
                case UNKNOWN_CLIENT:
                case DUPLICATE_CLIENT:
                    // not going to happen in RWS
                    JK_S( json_result,  kResult, "ERROR" );
                    break;
                case INVALID_REQUEST:
                    JK_S( json_result,  kResult, "ERROR" );
                    break;
            };
            JK_S( json_result,  kParams, result.json_params);
        }
    }
    else if( req->url_.compare(0, sizeof(URL_MESSAGE_CMD) - 1,
                URL_MESSAGE_CMD) == 0 ) {
        if( MessageRestCall( req, res, result) == false ) {
            res->status_ = 400;  //
            res->mime_ = "application/json";
            res->response_ = "";
            return true;
        }
    }
    else if( req->url_.compare(0, sizeof(URL_LEAVE_CMD) - 1,
                URL_LEAVE_CMD) == 0 ) {
        if( LeaveRestCall( req, res, result) == false ) {
            res->status_ = 400;  //
            res->mime_ = "application/json";
            res->response_ = "";
            return true;
        }
    }
    JK_S( json_result,  kResult, "SUCCESS" );
    res->status_ = 200;
    res->mime_ = "application/json";
    res->response_ = writer.write(json_result);

    return true;
}

bool AppClient::DoGet(HttpRequest* req, HttpResponse* res)  {
    RTC_LOG(INFO) << __FUNCTION__ << ", URL : " << req->url_;
    Json::StyledWriter writer;
    Json::Value json_result;
    Result result;

    JK_S( json_result,  kResult, "SUCCESS" );
    res->status_ = 200;
    res->mime_ = "application/json";
    res->response_ = writer.write(json_result);
    return true;
}

AppClient::~AppClient() {
}

