/*
Copyright (c) 2018, rpi-webrtc-streamer Lyu,KeunChang

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
#include "app_ws_client.h"
#include "mmal_wrapper.h"
#include "utils.h"

//
// Extended JSON message keyword for request/response
//
// Event Message format
//
// The event message is a one-way message that is sent to the client
// when there is an error or other problem in the message
//
// { cmd : event, type: error, mesg: '...' }
// { cmd : event, type: notice, mesg: '...' }
//
// - Request/Rsponse format
//
// request / response is a message exchange format in which the client sends
// a request to the server, the server processes the received request,
// and delivers the processed message to the client in response format.
//
// 1. query client deviceid example
//
// { cmd : request, type: deviceid }
// { cmd : response, type: deviceid, data: { ... }, result: 'SUCCESS/FAILED',
//          error: '...' }
//
// 2. Query/Set media config
//
// { cmd : request, type: config, deviceid: deviceid, data : { ... }  }
// { cmd : response, type: config, data : { ... }, result: 'SUCCESS/FAILED',
//          error: '...' }
//
//      data:
//          Json format : media config
//          'save' : save updated media config
//          'read' : listing media config
//          'apply' : apply current media config
//              (applied immediately if rtc session is started.)
//          'reset-to-default' : reset all media configuration to default
//

// message keywords
static const char kKeyCmd[] = "cmd";
static const char kValueCmdRegister[] = "register";
static const char kValueCmdSend[] = "send";

static const char kKeyRegisterRoomId[] = "roomid";
static const char kKeyRegisterClientId[] = "clientid";
static const char kKeySendMessage[] = "msg";
static const char kKeySendType[] = "type";
static const char kValueCmdSendTypeBye[] = "bye";
static const char kValueCmdRequest[] = "request";
static const char kValueCmdResponse[] = "response";
static const char kValueCmdEvent[] = "event";

static const char kKeyCmdType[] = "type";
static const char kValueTyepDeviceId[] = "deviceid";
// media config version
static const char kValueTyepMcVersion[] = "mcversion";
static const char kValueTypeConfig[] = "config";
static const char kValueTypeError[] = "error";
static const char kValueTypeNotice[] = "notice";

static const char kKeyEventMesg[] = "mesg";

static const char kKeyDeviceId[] = "deviceid";
static const char kKeyData[] = "data";

static const char kValueDataSave[] = "save";
static const char kValueDataRead[] = "read";
static const char kValueDataReset[] = "reset-to-default";
static const char kValueDataApply[] = "apply";

static const char kKeyRequestResult[] = "result";
static const char kValueResultSuccess[] = "SUCCESS";
static const char kValueResultFailed[] = "FAILED";
static const char kKeyRequestError[] = "error";

//
//  Media Config Version
//
static const char kMediaConfigVersion[] = "v0.73";

//
// Error messages
//
static const char kErrDeviceIdMissing[] = "device ID not found";
static const char kErrDataKeyMissing[] = "data key missing";
static const char kErrInternalError[] = "Unknown Internal Error";
static const char kErrClientOrRoomIdNotFound[]
        = "No Client/Room ID found in register command";
static const char kErrRegisterClientOrRoomId[]
        = "Failed to register Client/Room ID in clientinfo";
static const char kErrInvalidJsonMessage[]
        = "Failed to parse Json Message in send command";
static const char kNotiSessionAlreadyOccupied[]
        = "Streamer session is already in use by another user";
static const char kErrMessageEmpy[] = "No json message in cmd send";
static const char kErrUnknownCommandType[] = "Unknown Command Type";
static const char kErrUnknownRequestType[] = "Unknown Request Type";
static const char kErrUnknownProtocolMessage[] = "Unknown Protocol Message";

////////////////////////////////////////////////////////////////////////////////
//
// App Websocket only Channel
//
////////////////////////////////////////////////////////////////////////////////
AppWsClient::AppWsClient()
    : websocket_message_(nullptr),num_chunked_frames_(0) {
    deviceid_inited_ = utils::GetHardwareDeviceId(&deviceid_);
    config_media_ = ConfigMediaSingleton::Instance();
    if( deviceid_inited_ )
        RTC_LOG(INFO) << "Getting Device ID : " << deviceid_;
    else
        RTC_LOG(LS_ERROR) << "Failed to get device id";
}

void AppWsClient::RegisterWebSocketMessage(WebSocketMessage *server_instance) {
    websocket_message_ = server_instance;
}

void AppWsClient::OnConnect(int sockid) {
    RTC_LOG(INFO) << "New WebSocket connnection id : " << sockid;
    // reset the chunked_frames list container
    chunked_frames_.clear();
    num_chunked_frames_=0;
}


bool AppWsClient::OnMessage(int sockid, const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << sockid << ")";
    RTC_DCHECK( config_media_ != nullptr );

    Json::Reader json_reader;
    Json::Value json_value;
    std::string cmd;
    bool parsing_successful;

    // There is an issue where Chrome & Firefox WebSocket sends json messages
    // in multiple chunks, so this is the part to solve.
    // If JSON parsing fails, it will be kept in chunked_frrames up to 5 times and
    // try to parse the collected chunked_frames again until the next message succeeds.
    if((parsing_successful = json_reader.parse(message, json_value)) == true){
        rtc::GetStringFromJsonObject(json_value, kKeyCmd, &cmd);
    };

    if ( parsing_successful && !cmd.empty() ) {
        // parsing success and found the cmd keyword...
        RTC_LOG(INFO) << "JSON Parsing Success: " << message;
    }
    else {
        static const int kMaxChunkedFrames=5;

        // json_value.clear();
        chunked_frames_.append(message);
        RTC_LOG(INFO) << "Chunked Frame (" << num_chunked_frames_ << "), Message : "
            << chunked_frames_;
        if (!json_reader.parse(chunked_frames_, json_value)) {
            // parsing failed
            if( num_chunked_frames_++ > kMaxChunkedFrames )  {
                RTC_LOG(INFO) << "Failed to parse, Dropping Chunked frames: "
                    << chunked_frames_;
                num_chunked_frames_ = 0;
                chunked_frames_.clear();
            }
            return true;
        }

        rtc::GetStringFromJsonObject(json_value, kKeyCmd, &cmd);
        if( cmd.empty() ) {
            return true;
        }
        // parsing success and cmd keyword found.
        RTC_LOG(INFO) << "Chunked frames successful: " << chunked_frames_;
        num_chunked_frames_ = 0;
        chunked_frames_.clear();
        // finally successful parsing
    }

    rtc::GetStringFromJsonObject(json_value, kKeyCmd, &cmd);
    if( !cmd.empty() ) {    // found cmd id
        // command register
        if(cmd.compare(kValueCmdRegister)== 0) {
            int client_id, room_id;
            if( !rtc::GetIntFromJsonObject(json_value, kKeyRegisterRoomId, &room_id) ||
                !rtc::GetIntFromJsonObject(json_value, kKeyRegisterClientId, &client_id)) {
                RTC_LOG(LS_ERROR) << "Not found clientid/roomid :" << message;
                SendEvent(sockid, false, kErrClientOrRoomIdNotFound);
                return true;
            }
            RTC_LOG(INFO) << "Room ID: " << room_id << ", Client ID: " << client_id;
            if( app_client_.Registered( sockid, room_id, client_id ) == false ) {
                RTC_LOG(LS_ERROR) << "Failed to set room_id/client_id :" << message;
                // TODO: When sending an event message instead of connection close,
                // need to add error handling when receiving event messages
                // from client javascript.
                // SendEvent(sockid, true, kNotiSessionAlreadyOccupied);
                return false;
            };

            if ( IsStreamSessionActive() == false ) {
                if( ActivateStreamSession(client_id,utils::IntToString(client_id))
                        == true ) {
                    RTC_LOG(INFO) << "New WebSocket Name: " << client_id;
                };
            }
            else  {
                RTC_LOG(LS_ERROR) << "stream session already used by another user"
                    << message;
            }
            RTC_LOG(INFO) << "Internal Error" << client_id;
            SendEvent(sockid, false, kErrInternalError);
            return true;
        }
        // command send
        else if(cmd.compare(kValueCmdSend) == 0) {
            std::string msg;
            rtc::GetStringFromJsonObject(json_value, kKeySendMessage, &msg);
            if( !msg.empty() ) {
                Json::Value json_msg_value;
                std::string json_msg_type;

                // trying to parse msg value to check whether it is json type msg
                if (!json_reader.parse(msg, json_msg_value)) {
                    RTC_LOG(WARNING) << "Failed to parse send message string. "
                        << msg;
                    SendEvent(sockid, false, kErrInvalidJsonMessage);
                    return false;
                }

                rtc::GetStringFromJsonObject(json_msg_value, kKeySendType,
                        &json_msg_type);
                // checking send command is type:bye
                if( !json_msg_type.empty() &&
                    json_msg_type.compare(kValueCmdSendTypeBye) == 0) {
                    // command 'send' message is type: bye
                    // reset the app_clientinfo and deactivate the streaming
                    if( app_client_.IsRegistered( sockid ) == true ) {
                        app_client_.Reset();
                        if ( IsStreamSessionActive() == true ) {
                            DeactivateStreamSession();
                        };
                    };
                    return true;
                };

                MessageFromPeer(msg);
                return true;
            }
            SendEvent(sockid, false, kErrMessageEmpy);
            RTC_LOG(LS_ERROR) << "Failed to pass received message :" << message;
        }
        // command request
        else if(cmd.compare(kValueCmdRequest) == 0) {
            //  cmd : request, ...
            std::string cmd_type;
            rtc::GetStringFromJsonObject(json_value, kKeyCmdType, &cmd_type);
            if( cmd_type.compare(kValueTyepDeviceId)  == 0 ){
                SendDeviceIdResponse(sockid, deviceid_inited_, deviceid_);
                return true;
            }
            else if( cmd_type.compare(kValueTypeConfig)  == 0 ){
                // { cmd : request, type: config, deviceid: deviceid,
                //      data : { ... }  }
                std::string deviceid;
                std::string data;
                std::string json_error;
                std::string updated_config;

                //
                // Verify that the deviceid key exists
                //
                if( rtc::GetStringFromJsonObject(json_value, kKeyDeviceId,
                            &deviceid ) == false ) {
                    SendMediaConfigResponse(sockid,
                            false, "", kErrDeviceIdMissing );
                    return true;
                }
                else if( rtc::GetStringFromJsonObject(json_value, kKeyData,
                            &data ) == false ) {
                    // data key is not found;
                    RTC_LOG(LS_ERROR) << "Failed to get data key";
                    SendMediaConfigResponse(sockid,
                            false, "", kErrDataKeyMissing );
                    return true;
                }

                //
                // Read Command
                //
                if( data.compare( kValueDataRead ) == 0 ) {
                    // sends the entire media_config to the client.
                    std::string media_config;

                    config_media_->ConfigToJson(media_config);
                    SendMediaConfigResponse(sockid, true, media_config, "");
                    return true;
                }
                //
                // Save Command
                //
                else if( data.compare( kValueDataSave ) == 0 ) {
                    if(config_media_->Save() == false){
                        RTC_LOG(LS_ERROR) << "Failed to save media config";
                        SendMediaConfigResponse(sockid, false, "{}", "");
                        return true;
                    }
                    else {
                        // save successufl,
                        // and sends the entire media_config to the client.
                        std::string media_config;
                        config_media_->ConfigToJson(media_config);
                        SendMediaConfigResponse(sockid, true, media_config, "");
                        return true;
                    }
                }
                //
                // Reset Command
                //
                else if( data.compare( kValueDataReset ) == 0 ) {
                    // reset to default media configurations
                    config_media_->LoadConfigWithDefault();

                    // sends the entire media_config to the client.
                    std::string media_config;
                    config_media_->ConfigToJson(media_config);
                    SendMediaConfigResponse(sockid, true, media_config, "");
                    return true;
                }
                //
                // Apply Command
                //
                else if( data.compare( kValueDataApply ) == 0 ) {
                    if ( IsStreamSessionActive() == true ) {
                        webrtc::MMALWrapper::Instance()->SetMediaConfigParams();
                        if( webrtc::MMALWrapper::Instance()->ReinitEncoderInternal()
                                == true){
                            RTC_LOG(INFO) << "ReinitEncoderInternal Success";
                            // restart capture
                            webrtc::MMALWrapper::Instance()->StartCapture();
                            SendMediaConfigResponse(sockid, true, "", "");
                        }
                        else {
                            RTC_LOG(LS_ERROR) << "Failed to ReinitEncoderInternal";
                            SendMediaConfigResponse(sockid, false, "", "");
                        }
                    }
                    else {
                        // do not neeed to init the entire video encoding
                        SendMediaConfigResponse(sockid, true, "", "");
                    }
                    return true;
                }

                if(config_media_->ConfigFromJson(data, &updated_config,
                            json_error) == false){
                    RTC_LOG(LS_ERROR) << "Failed to parse config data";
                    SendMediaConfigResponse(sockid, false, "", json_error);
                }
                else {
                    RTC_LOG(INFO) << "Media Config : " << updated_config;
                    SendMediaConfigResponse(sockid, true, updated_config, "");
                }
                return true;
            }
            RTC_LOG(LS_ERROR) << "Unknown Request command type: " << cmd_type;
            SendEvent(sockid, false, kErrUnknownRequestType);
        }
        SendEvent(sockid, false, kErrUnknownCommandType);
        return true;
    };
    RTC_LOG(LS_ERROR) << "Received unknown protocol message. " << message;
    SendEvent(sockid, false, kErrUnknownProtocolMessage);
    return true;
}


void AppWsClient::OnDisconnect(int sockid) {
    RTC_LOG(INFO) << "WebSocket connnection id : " << sockid << " closed";
    // Ignore if websocket id is not the registered websocket id.
    if( app_client_.DisconnectWait(sockid) == true ) {
        app_client_.Reset();
        if ( IsStreamSessionActive() == true ) {
            DeactivateStreamSession();
        };
    }
}

void AppWsClient::OnError(int sockid, const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__ << "Called : " << sockid;
}

bool AppWsClient::SendMessageToPeer(const int peer_id, const std::string &message) {
    RTC_DCHECK(websocket_message_ != nullptr ) << "WebSocket Server instance is nullptr";
    int websocket_id;

    RTC_LOG(INFO) << __FUNCTION__;
    if( app_client_.GetWebsocketId(peer_id, websocket_id ) == true ) {
        Json::StyledWriter json_writer;
        Json::Value json_message;

        json_message[kKeyCmd] = kValueCmdSend;
        json_message[kKeySendMessage] = message;
        websocket_message_->SendMessage(websocket_id, json_writer.write(json_message));
        return true;
    }
    return false;
}

void AppWsClient::SendDeviceIdResponse(int sockid, bool success,
        const std::string& deviceid) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(websocket_message_ != nullptr ) << "WebSocket Server instance is nullptr";
    std::string json_resp_mesg;
    Json::StyledWriter json_writer;
    Json::Value json_response;
    Json::Value json_data;

    // response data
    json_data[kValueTyepDeviceId] = deviceid;
    json_data[kValueTyepMcVersion] = kMediaConfigVersion;

    json_response[kKeyCmd] = kValueCmdResponse;
    json_response[kKeyCmdType] = kValueTyepDeviceId;
    if(  success == true )  {
        json_response[kKeyData] = json_writer.write(json_data);
        json_response[kKeyRequestResult] = kValueResultSuccess;
    }
    else {
        json_response[kKeyRequestResult] = kValueResultFailed;
    }
    json_resp_mesg =  json_writer.write(json_response);
    websocket_message_->SendMessage(sockid, json_resp_mesg );
    RTC_LOG(INFO) << "Device ID JSON response: " << json_resp_mesg;
}

void AppWsClient::SendMediaConfigResponse(int sockid, bool success,
        const std::string& media_config, const std::string& error_mesg) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(websocket_message_ != nullptr ) << "WebSocket Server instance is nullptr";
    std::string json_resp_mesg;
    Json::StyledWriter json_writer;
    Json::Value json_response;

    json_response[kKeyCmd] = kValueCmdResponse;
    json_response[kKeyCmdType] = kValueTypeConfig;
    json_response[kKeyData] = media_config;
    json_response[kKeyRequestError] = error_mesg;
    if(  success == true )  {
        json_response[kKeyRequestResult] = kValueResultSuccess;
        json_response[kKeyRequestError] = "";
    }
    else {
        json_response[kKeyRequestResult] = kValueResultFailed;
        json_response[kKeyRequestError] = error_mesg;
    }
    json_resp_mesg =  json_writer.write(json_response);
    websocket_message_->SendMessage(sockid, json_resp_mesg );
    RTC_LOG(INFO) << "Media Config JSON response : " << json_resp_mesg;
}

void AppWsClient::SendEvent(int sockid, bool is_notice,
            const std::string& event_mesg) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(websocket_message_ != nullptr ) << "WebSocket Server instance is nullptr";
    std::string json_resp_mesg;
    Json::StyledWriter json_writer;
    Json::Value json_response;

    json_response[kKeyCmd] = kValueCmdEvent;
    json_response[kKeyEventMesg] = event_mesg;
    if(  is_notice == true )  {
        json_response[kKeyCmdType] = kValueTypeNotice;
    }
    else {
        json_response[kKeyCmdType] = kValueTypeError;
    }
    json_resp_mesg =  json_writer.write(json_response);
    websocket_message_->SendMessage(sockid, json_resp_mesg );
    RTC_LOG(INFO) << "Event Message to Client: " << json_resp_mesg;
}

AppWsClient::~AppWsClient() {
}

