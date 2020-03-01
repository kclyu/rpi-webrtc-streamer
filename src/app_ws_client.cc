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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "app_ws_client.h"

#include <memory>

#include "mmal_wrapper.h"
#include "utils.h"
#include "websocket_server.h"

//
// Extended JSON message keyword for request/response
//
// - Event Message format
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
// 2. query RTCPeerConnnection config
//
// { cmd : request, type: rtcconfig }
// { cmd : response, type: rtcconfig, data: { ... }, result: 'SUCCESS/FAILED',
//          error: '...' }
//
//
// 3. Query/Set media config
//
// { cmd : request, type: config, deviceid: deviceid, data : { ... }  }
// { cmd : response, type: config, data : { ... }, result: 'SUCCESS/FAILED',
//          error: '...' }
//      data:
//          Json format : media config
//          'save' : save updated media config
//          'read' : listing media config
//          'apply' : apply current media config
//              (applied immediately if rtc session is started.)
//          'reset-to-default' : reset all media configuration to default
//
// - Message format
//
// Similar to the send used for signaling. but, forwarding messages
// that are not used for signaling purposes. There is no response from the
// message received, and the error is forwarded to the event.
//
// { cmd: message, type: "zoom", data: { x: value, y: value, command:
// command_type } }
//      value: double type
//      command: in/out/reset
//

// message keywords
static const char kKeyCmd[] = "cmd";

// notice/error event
static const char kValueCmdEvent[] = "event";
static const char kValueTypeError[] = "error";
static const char kValueTypeNotice[] = "notice";

// register
static const char kValueCmdRegister[] = "register";
static const char kKeyRegisterRoomId[] = "roomid";
static const char kKeyRegisterClientId[] = "clientid";

// send
static const char kValueCmdSend[] = "send";
static const char kKeySendMessage[] = "msg";
static const char kKeySendType[] = "type";
static const char kValueCmdSendTypeBye[] = "bye";

// request/response
static const char kValueCmdRequest[] = "request";
static const char kValueCmdResponse[] = "response";

static const char kKeyCmdType[] = "type";
static const char kValueTyepDeviceId[] = "deviceid";
static const char kValueTyepMcVersion[] = "mcversion";

static const char kValueTypeRTCConfig[] = "rtcconfig";
static const char kValueTypeConfig[] = "config";

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

// { cmd: message, type: "zoom", data: { x: value, y: value, command:
// command_type } }
//      value: double type
//      command: in/out/reset
//
// request/response

static const char kValueCmdMessage[] = "message";
static const char kKeyDataCommand[] = "command";
static const char kValueTypeZoom[] = "zoom";
static const char kValueDataX[] = "x";
static const char kValueDataY[] = "y";
static const char kValueCommandZoomIn[] = "in";
static const char kValueCommandZoomOut[] = "out";
static const char kValueCommandZoomReset[] = "reset";
static const char kValueCommandZoomMove[] = "move";

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
static const char kErrClientOrRoomIdNotFound[] =
    "No Client/Room ID found in register command";
static const char kErrRegisterClientOrRoomId[] =
    "Failed to register Client/Room ID in clientinfo";
static const char kErrInvalidJsonMessage[] =
    "Failed to parse Json Message in send command";
static const char kNotiSessionAlreadyOccupied[] =
    "Streamer session is already in use by another user";
static const char kErrMessageEmpy[] = "No json message in cmd send";
static const char kErrUnknownCommandType[] = "Unknown Command Type";
static const char kErrUnknownRequestType[] = "Unknown Request Type";
static const char kErrUnknownProtocolMessage[] = "Unknown Protocol Message";
static const char kErrRTCConfig[] = "Failed to get RTC Configuration";

// delay of message to use for stream release
static const int kStreamReleaseDelay = 1000;

////////////////////////////////////////////////////////////////////////////////
//
// App Websocket only Channel
//
////////////////////////////////////////////////////////////////////////////////
AppWsClient::AppWsClient()
    : websocket_message_(nullptr), num_chunked_frames_(0) {
    deviceid_inited_ = utils::GetHardwareDeviceId(&deviceid_);
    config_media_ = ConfigMediaSingleton::Instance();
    streamer_config_ = nullptr;
    if (deviceid_inited_)
        RTC_LOG(INFO) << "Using Device ID : " << deviceid_;
    else
        RTC_LOG(LS_ERROR) << "Failed to get device id";
}

void AppWsClient::RegisterWebSocketMessage(WebSocketMessage* server_instance) {
    websocket_message_ = server_instance;
}

void AppWsClient::RegisterConfigStreamer(StreamerConfig* streamer_config) {
    streamer_config_ = streamer_config;
}

void AppWsClient::OnConnect(int sockid) {
    RTC_LOG(INFO) << "New WebSocket connnection id : " << sockid;
    // reset the chunked_frames list container
    chunked_frames_.clear();
    num_chunked_frames_ = 0;
}

bool AppWsClient::OnMessage(int sockid, const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << sockid << ")";
    RTC_DCHECK(config_media_ != nullptr);

    Json::Reader json_reader;
    Json::Value json_value;
    std::string cmd;
    bool parsing_successful;

    // There is an issue where Chrome & Firefox WebSocket sends json messages
    // in multiple chunks, so this is the part to solve.
    // If JSON parsing fails, it will be kept in chunked_frrames up to 5 times
    // and try to parse the collected chunked_frames again until the next
    // message succeeds.
    if ((parsing_successful = json_reader.parse(message, json_value)) == true) {
        rtc::GetStringFromJsonObject(json_value, kKeyCmd, &cmd);
    };

    if (parsing_successful && !cmd.empty()) {
        // parsing success and found the cmd keyword...
        RTC_LOG(INFO) << "JSON Parsing Success: " << message;
    } else {
        static const int kMaxChunkedFrames = 5;

        // json_value.clear();
        chunked_frames_.append(message);
        RTC_LOG(INFO) << "Chunked Frame (" << num_chunked_frames_
                      << "), Message : " << chunked_frames_;
        if (!json_reader.parse(chunked_frames_, json_value)) {
            // parsing failed
            if (num_chunked_frames_++ > kMaxChunkedFrames) {
                RTC_LOG(INFO) << "Failed to parse, Dropping Chunked frames: "
                              << chunked_frames_;
                num_chunked_frames_ = 0;
                chunked_frames_.clear();
            }
            return true;
        }

        rtc::GetStringFromJsonObject(json_value, kKeyCmd, &cmd);
        if (cmd.empty()) {
            return true;
        }
        // parsing success and cmd keyword found.
        RTC_LOG(INFO) << "Chunked frames successful: " << chunked_frames_;
        num_chunked_frames_ = 0;
        chunked_frames_.clear();
        // finally successful parsing
    }

    rtc::GetStringFromJsonObject(json_value, kKeyCmd, &cmd);
    if (!cmd.empty()) {  // found cmd id
        // command register
        if (cmd.compare(kValueCmdRegister) == 0) {
            int client_id, room_id;
            if (!rtc::GetIntFromJsonObject(json_value, kKeyRegisterRoomId,
                                           &room_id) ||
                !rtc::GetIntFromJsonObject(json_value, kKeyRegisterClientId,
                                           &client_id)) {
                RTC_LOG(LS_ERROR) << "Not found clientid/roomid :" << message;
                SendEvent(sockid, EventError, kErrClientOrRoomIdNotFound);
                return true;
            }
            RTC_LOG(INFO) << "Room ID: " << room_id
                          << ", Client ID: " << client_id;
            if (app_client_.Registered(sockid, room_id, client_id) == false) {
                RTC_LOG(LS_ERROR)
                    << "Failed to set room_id/client_id :" << message;
                SendEvent(sockid, EventNotice, kNotiSessionAlreadyOccupied);
                // In RWS, the connection is maintained, but in a client that
                // receives an error event, the connection must be disconnected.
                return true;
            };

            if (IsStreamSessionActive() == false) {
                if (ActivateStreamSession(
                        client_id, utils::IntToString(client_id)) == true) {
                    RTC_LOG(INFO) << "New WebSocket Name: " << client_id;
                };
                return true;
            } else {
                RTC_LOG(LS_ERROR)
                    << "Internal Error, The connection is terminated"
                       " and the internal variables are reset : "
                    << message;
            }
            RTC_LOG(INFO) << "Internal Error" << client_id;
            SendEvent(sockid, EventError, kErrInternalError);
            return false;  // closing connection
        }
        // command send
        else if (cmd.compare(kValueCmdSend) == 0) {
            std::string msg;
            rtc::GetStringFromJsonObject(json_value, kKeySendMessage, &msg);

            if (!msg.empty()) {
                Json::Value json_msg_value;
                std::string json_msg_type;

                // trying to parse msg value to check whether it is json type
                // msg
                if (!json_reader.parse(msg, json_msg_value)) {
                    RTC_LOG(WARNING)
                        << "Failed to parse send message string. " << msg;
                    SendEvent(sockid, EventError, kErrInvalidJsonMessage);
                    return false;  // closing connection
                }

                rtc::GetStringFromJsonObject(json_msg_value, kKeySendType,
                                             &json_msg_type);
                // checking send command is type:bye
                if (!json_msg_type.empty() &&
                    json_msg_type.compare(kValueCmdSendTypeBye) == 0) {
                    // command 'send' message is type: bye
                    // reset the app_clientinfo and deactivate the streaming
                    if (app_client_.IsRegistered(sockid) == true) {
                        app_client_.Reset();
                        if (IsStreamSessionActive() == true) {
                            DeactivateStreamSession();
                        };
                    };
                    return true;
                };

                MessageFromPeer(msg);
                return true;
            }
            SendEvent(sockid, EventError, kErrMessageEmpy);
            RTC_LOG(LS_ERROR) << "Failed to pass received message :" << message;
        }

        // command message
        else if (cmd.compare(kValueCmdMessage) == 0) {
            //  cmd : message ...
            std::string cmd_type;
            rtc::GetStringFromJsonObject(json_value, kKeyCmdType, &cmd_type);
            if (cmd_type.compare(kValueTypeZoom) == 0) {
                std::string data;
                rtc::GetStringFromJsonObject(json_value, kKeyData, &data);
                if (!data.empty()) {
                    Json::Value json_data_value;
                    // trying to parse msg value to check whether it is json
                    // type msg
                    if (!json_reader.parse(data, json_data_value)) {
                        RTC_LOG(LS_ERROR)
                            << "Failed to parse data message: " << data;
                        return true;
                    }

                    double cx = 0, cy = 0;
                    std::string command;
                    rtc::GetStringFromJsonObject(json_data_value,
                                                 kKeyDataCommand, &command);
                    if (command.empty()) {
                        RTC_LOG(LS_ERROR)
                            << "Failed to get zoom command: " << message;
                        return true;
                    }
                    if (rtc::GetDoubleFromJsonObject(
                            json_data_value, kValueDataX, &cx) == false ||
                        rtc::GetDoubleFromJsonObject(
                            json_data_value, kValueDataY, &cy) == false) {
                        RTC_LOG(LS_ERROR)
                            << "Failed to get cx/cy position : " << message;
                        return true;
                    }
                    RTC_LOG(INFO) << "Zoom Command Type " << command
                                  << ", Position " << cx << ", " << cy;
                    if (command.compare(kValueCommandZoomIn) == 0) {
                        webrtc::MMALWrapper::Instance()->IncreaseDigitalZoom(
                            cx, cy);
                    } else if (command.compare(kValueCommandZoomOut) == 0) {
                        webrtc::MMALWrapper::Instance()->DecreaseDigitalZoom();
                    } else if (command.compare(kValueCommandZoomReset) == 0) {
                        webrtc::MMALWrapper::Instance()->ResetDigitalZoom();
                    } else if (command.compare(kValueCommandZoomMove) == 0) {
                        webrtc::MMALWrapper::Instance()->MoveDigitalZoom(cx,
                                                                         cy);
                    }
                } else {
                    RTC_LOG(LS_ERROR)
                        << "Failed to get data key in message :" << message;
                }
            }
            return true;
        }

        // command request
        else if (cmd.compare(kValueCmdRequest) == 0) {
            //  cmd : request, ...
            std::string cmd_type;
            rtc::GetStringFromJsonObject(json_value, kKeyCmdType, &cmd_type);
            //
            // Device ID request
            //
            if (cmd_type.compare(kValueTyepDeviceId) == 0) {
                SendResponseDeviceId(sockid, deviceid_inited_, deviceid_);
                return true;
            }
            //
            // RTC Config request
            //
            else if (cmd_type.compare(kValueTypeRTCConfig) == 0) {
                // { cmd : response, type: rtcconfig, data: { ... }, result:
                // 'SUCCESS/FAILED',
                //          error: '...' }
                std::string deviceid;
                std::string json_rtcconfig;
                std::string json_error;
                std::string updated_config;

                //
                // Verify that the deviceid key exists
                //
                if (rtc::GetStringFromJsonObject(json_value, kKeyDeviceId,
                                                 &deviceid) == false) {
                    SendResponse(sockid, false, kValueTypeConfig, "",
                                 kErrDeviceIdMissing);
                    return true;
                };

                if (streamer_config_->GetRTCConfig(json_rtcconfig) == true) {
                    RTC_LOG(INFO) << "JSON RTC Config : " << json_rtcconfig;
                    SendResponse(sockid, true, kValueTypeRTCConfig,
                                 json_rtcconfig, "");
                } else {
                    RTC_LOG(INFO) << "Failed to get JSON RTC Config";
                    SendResponse(sockid, false, kValueTypeRTCConfig, "",
                                 kErrRTCConfig);
                }
                return true;
            }
            //
            // Config request
            //

            else if (cmd_type.compare(kValueTypeConfig) == 0) {
                // { cmd : request, type: config, deviceid: deviceid,
                //      data : { ... }  }
                std::string deviceid;
                std::string data;
                std::string json_error;
                std::string updated_config;

                //
                // Verify that the deviceid key exists
                //
                if (rtc::GetStringFromJsonObject(json_value, kKeyDeviceId,
                                                 &deviceid) == false) {
                    SendResponse(sockid, false, kValueTypeConfig, "",
                                 kErrDeviceIdMissing);
                    return true;
                } else if (rtc::GetStringFromJsonObject(json_value, kKeyData,
                                                        &data) == false) {
                    // data key is not found;
                    RTC_LOG(LS_ERROR) << "Failed to get data key";
                    SendResponse(sockid, false, kValueTypeConfig, "",
                                 kErrDataKeyMissing);
                    return true;
                }

                //
                // Read Command
                //
                if (data.compare(kValueDataRead) == 0) {
                    // sends the entire media_config to the client.
                    std::string media_config;

                    config_media_->ConfigToJson(media_config);
                    SendResponse(sockid, true, kValueTypeConfig, media_config,
                                 "");
                    return true;
                }
                //
                // Save Command
                //
                else if (data.compare(kValueDataSave) == 0) {
                    if (config_media_->Save() == false) {
                        RTC_LOG(LS_ERROR) << "Failed to save media config";
                        SendResponse(sockid, false, kValueTypeConfig, "{}", "");
                        return true;
                    } else {
                        // save successufl,
                        // and sends the entire media_config to the client.
                        std::string media_config;
                        config_media_->ConfigToJson(media_config);
                        SendResponse(sockid, true, kValueTypeConfig,
                                     media_config, "");
                        return true;
                    }
                }
                //
                // Reset Command
                //
                else if (data.compare(kValueDataReset) == 0) {
                    // reset to default media configurations
                    config_media_->LoadConfigWithDefault();

                    // sends the entire media_config to the client.
                    std::string media_config;
                    config_media_->ConfigToJson(media_config);
                    SendResponse(sockid, true, kValueTypeConfig, media_config,
                                 "");
                    return true;
                }
                //
                // Apply Command
                //
                else if (data.compare(kValueDataApply) == 0) {
                    if (IsStreamSessionActive() == true) {
                        webrtc::MMALWrapper::Instance()->SetMediaConfigParams();
                        if (webrtc::MMALWrapper::Instance()
                                ->ReinitEncoderInternal() == true) {
                            RTC_LOG(INFO) << "ReinitEncoderInternal Success";
                            // restart capture
                            webrtc::MMALWrapper::Instance()->StartCapture();
                            SendResponse(sockid, true, kValueTypeConfig, "",
                                         "");
                        } else {
                            RTC_LOG(LS_ERROR)
                                << "Failed to ReinitEncoderInternal";
                            SendResponse(sockid, false, kValueTypeConfig, "",
                                         "");
                        }
                    } else {
                        // do not neeed to init the entire video encoding
                        SendResponse(sockid, true, kValueTypeConfig, "", "");
                    }
                    return true;
                }

                if (config_media_->ConfigFromJson(data, &updated_config,
                                                  json_error) == false) {
                    RTC_LOG(LS_ERROR) << "Failed to parse config data";
                    SendResponse(sockid, false, kValueTypeConfig, "",
                                 json_error);
                } else {
                    RTC_LOG(INFO) << "Media Config : " << updated_config;
                    SendResponse(sockid, true, kValueTypeConfig, updated_config,
                                 "");
                }
                return true;
            }
            RTC_LOG(LS_ERROR) << "Unknown Request command type: " << cmd_type;
            SendEvent(sockid, EventError, kErrUnknownRequestType);
        }
        SendEvent(sockid, EventError, kErrUnknownCommandType);
        return true;
    };
    RTC_LOG(LS_ERROR) << "Received unknown protocol message. " << message;
    SendEvent(sockid, EventError, kErrUnknownProtocolMessage);
    return true;
}

void AppWsClient::OnDisconnect(int sockid) {
    RTC_LOG(INFO) << "WebSocket connnection id : " << sockid << " closed";
    // Ignore if websocket id is not the registered websocket id.
    if (app_client_.DisconnectWait(sockid) == true) {
        app_client_.Reset();
        if (IsStreamSessionActive() == true) {
            DeactivateStreamSession();
        };
    }
}

void AppWsClient::OnError(int sockid, const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__ << "Called : " << sockid;
}

bool AppWsClient::SendMessageToPeer(const int peer_id,
                                    const std::string& message) {
    RTC_DCHECK(websocket_message_ != nullptr)
        << "WebSocket Server instance is nullptr";
    int websocket_id;

    RTC_LOG(INFO) << __FUNCTION__;
    if (app_client_.GetWebsocketId(peer_id, websocket_id) == true) {
        Json::StyledWriter json_writer;
        Json::Value json_message;

        json_message[kKeyCmd] = kValueCmdSend;
        json_message[kKeySendMessage] = message;
        websocket_message_->SendMessage(websocket_id,
                                        json_writer.write(json_message));
        return true;
    }
    return false;
}

void AppWsClient::ReportEvent(const int peer_id, bool drop_connection,
                              const std::string& message) {
    RTC_DCHECK(websocket_message_ != nullptr)
        << "WebSocket Server instance is nullptr";
    int websocket_id;

    RTC_LOG(INFO) << __FUNCTION__;

    if (app_client_.GetWebsocketId(peer_id, websocket_id) == true) {
        if (drop_connection == true) {
            rtc::Thread::Current()->PostDelayed(
                RTC_FROM_HERE, kStreamReleaseDelay, this, 3400,
                new rtc::TypedMessageData<int>(websocket_id));

            return;
        }
        RTC_LOG(INFO) << __FUNCTION__ << "Reporting Event : " << message
                      << ", Drop Connection?: " << drop_connection;
    }

    return;
}

void AppWsClient::OnMessage(rtc::Message* msg) {
    RTC_DCHECK(msg->message_id == 3400);
    rtc::TypedMessageData<int>* msg_data =
        static_cast<rtc::TypedMessageData<int>*>(msg->pdata);
    int websocket_id = msg_data->data();

    //  Internally, there is no reason to keep the session anymore,
    //  so it terminates the session that is currently being held.
    RTC_LOG(INFO) << __FUNCTION__
                  << "Drop WebSocket Connection : " << websocket_id;
    websocket_message_->Close(websocket_id, 0, "");
    if (app_client_.DisconnectWait(websocket_id) == true) {
        app_client_.Reset();
        if (IsStreamSessionActive() == true) {
            DeactivateStreamSession();
        };
    }
    return;
}

void AppWsClient::SendResponseDeviceId(int sockid, bool success,
                                       const std::string& deviceid) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(websocket_message_ != nullptr)
        << "WebSocket Server instance is nullptr";
    std::string json_resp_mesg;
    Json::StyledWriter json_writer;
    Json::Value json_response;
    Json::Value json_data;

    // response data
    json_data[kValueTyepDeviceId] = deviceid;
    json_data[kValueTyepMcVersion] = kMediaConfigVersion;

    json_response[kKeyCmd] = kValueCmdResponse;
    json_response[kKeyCmdType] = kValueTyepDeviceId;
    if (success == true) {
        json_response[kKeyData] = json_writer.write(json_data);
        json_response[kKeyRequestResult] = kValueResultSuccess;
    } else {
        json_response[kKeyRequestResult] = kValueResultFailed;
    }
    json_resp_mesg = json_writer.write(json_response);
    websocket_message_->SendMessage(sockid, json_resp_mesg);
    RTC_LOG(INFO) << "Device ID JSON response: " << json_resp_mesg;
}

void AppWsClient::SendResponse(int sockid, bool success,
                               const std::string& type, const std::string& data,
                               const std::string& error_mesg) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(websocket_message_ != nullptr)
        << "WebSocket Server instance is nullptr";
    std::string json_resp_mesg;
    Json::StyledWriter json_writer;
    Json::Value json_response;

    json_response[kKeyCmd] = kValueCmdResponse;
    json_response[kKeyCmdType] = kValueTypeConfig;
    json_response[kKeyData] = data;
    json_response[kKeyRequestError] = error_mesg;
    if (success == true) {
        json_response[kKeyRequestResult] = kValueResultSuccess;
        json_response[kKeyRequestError] = "";
    } else {
        json_response[kKeyRequestResult] = kValueResultFailed;
        json_response[kKeyRequestError] = error_mesg;
    }
    json_resp_mesg = json_writer.write(json_response);
    websocket_message_->SendMessage(sockid, json_resp_mesg);
    RTC_LOG(INFO) << "Media Config JSON response : " << json_resp_mesg;
}

void AppWsClient::SendEvent(int sockid, EventType type,
                            const std::string& event_mesg) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(websocket_message_ != nullptr)
        << "WebSocket Server instance is nullptr";
    std::string json_resp_mesg;
    Json::StyledWriter json_writer;
    Json::Value json_response;

    json_response[kKeyCmd] = kValueCmdEvent;
    json_response[kKeyEventMesg] = event_mesg;
    if (type == EventError) {
        json_response[kKeyCmdType] = kValueTypeError;
    } else {
        // type == EventNotice
        json_response[kKeyCmdType] = kValueTypeNotice;
    }

    json_resp_mesg = json_writer.write(json_response);
    websocket_message_->SendMessage(sockid, json_resp_mesg);
    RTC_LOG(INFO) << "Event Message to Client: " << json_resp_mesg;
}

AppWsClient::~AppWsClient() {}
