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

#include "absl/strings/str_cat.h"
#include "mmal_still_capture.h"
#include "mmal_wrapper.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/json.h"
#include "utils.h"
#include "websocket_server.h"

namespace {

// used as key for comand value
const char kKeyCmd[] = "cmd";
const char kKeyData[] = "data";
const char kKeyCmdType[] = "type";
const char kKeyTransaction[] = "transaction";

const char kKeyRequestResult[] = "result";
const char kValueResultSuccess[] = "SUCCESS";
const char kValueResultFailed[] = "FAILED";
const char kKeyRequestError[] = "error";

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
//
const char kValueCmdEvent[] = "event";
const char kValueTypeError[] = "error";
const char kValueTypeNotice[] = "notice";
const char kKeyEventMesg[] = "mesg";
//
// - Request/Rsponse format
//
// request / response is a message exchange format in which the client sends
// a request to the server, the server processes the received request,
// and delivers the processed message to the client in response format.
//
// 1. query client info
//
// { cmd : request, type: info }
// { cmd : response, type: info , data: { ... },
// 		result: 'SUCCESS/FAILED', error: '...' }
//
// 2. query RTCPeerConnnection config
//
// - get
// If the rws clinent does not have its own rtcconfig, it receives and uses
// the rtcconfig set in rws config.
// { cmd : request, type: rtcconfig, transaction: '...' }
// { cmd : response, type: rtcconfig, data: { ... }, transaction: '...',
// 		result: 'SUCCESS/FAILED', error: '...' }
//
// - set
// The rtcconfig configured through the websocket interface is valid
// only while the websocket connection is maintained. If websocket is
// disconnected, rtcconfig to be used must be reconfigured.
// { cmd : request, type: rtcconfig,  data: { ... }, transaction: '...'}
// { cmd : response, type: rtcconfig, , transaction: '...', result:
// 'SUCCESS/FAILED', error: '...' }
//
//
// 3. Query/Set media config
//
// { cmd : request, type: config, data : { ... }, transaction: '...'  }
// { cmd : response, type: config, data : { ... }, transaction: '...',
// 		result: 'SUCCESS/FAILED', error: '...' }
//
//      data:
//          Json format : media config
//          'save' : save updated media config
//          'read' : listing media config
//          'apply' : apply current media config
//              (applied immediately if rtc session is started.)
//          'reset-to-default' : reset all media configuration to default
//
// 4. capture still image
//
// { cmd : request, type: still, data : { ... }, transaction: '...'  }
//
//      data:
//          Json format : (all optional)
//          'width' : width of still image
//          'height' : height of still image
//          'cameraNum' : the id of camera ( 0: default, ...)
//          'filename' : the prefix of the filename to be used with captureing
//          'quality' : specify the quality of capture image [ the default value
//               of quality is 85 ] 'timeout' : the quality of capture image
//          'extension' : the quality of capture image
//          	['jpg', 'png', 'bmp',' gif']
//          'verbose' : internal usage only for debugging
//
// { cmd : response, type: still, data : { ... }, transaction: '...',
// 		result: 'SUCCESS/FAILED', error: '...' }
//
// 		data:
// 			json format: when result is 'SUCCESS'
// 			'filename' : the filename of captured image
// 			'url' : http url path (excluding protocol, hostname and port)
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
// - Signaling message format
//   - register

// register
const char kValueCmdRegister[] = "register";
const char kKeyRegisterRoomId[] = "roomid";
const char kKeyRegisterClientId[] = "clientid";

// send
const char kValueCmdSend[] = "send";
const char kKeySendMessage[] = "msg";
const char kKeySendType[] = "type";
const char kValueCmdSendTypeBye[] = "bye";

// request/response
const char kValueCmdRequest[] = "request";
const char kValueCmdResponse[] = "response";

const char kValueTypepDeviceInfo[] = "info";
const char kDataKeyDeviceId[] = "deviceid";
const char kDataKeyMcVersion[] = "mcversion";
const char kDataKeyStillCapture[] = "stillcapture";
const char kDataKeyCameraEnabled[] = "cameraenabled";
const char kDataKeyVideo43AspectRatio[] = "video43aspectratio";

const char kValueTypeRTCConfig[] = "rtcconfig";
const char kValueTypeConfig[] = "config";

const char kValueDataSave[] = "save";
const char kValueDataRead[] = "read";
const char kValueDataReset[] = "reset-to-default";
const char kValueDataApply[] = "apply";

// { cmd: message, type: "zoom", data: { x: value, y: value, command:
// command_type } }
//      value: double type
//      command: in/out/reset
//

const char kValueCmdMessage[] = "message";
const char kKeyDataCommand[] = "command";
const char kValueTypeZoom[] = "zoom";
const char kValueDataX[] = "x";
const char kValueDataY[] = "y";
const char kValueCommandZoomIn[] = "in";
const char kValueCommandZoomOut[] = "out";
const char kValueCommandZoomReset[] = "reset";
const char kValueCommandZoomMove[] = "move";

//
//  still capture
//
const char kValueTypeStill[] = "still";
const char kValueStillDataWidth[] = "width";
const char kValueStillDataHeight[] = "height";
const char kValueStillDataCameraNum[] = "cameraNum";
const char kValueStillDataFilename[] = "filename";  // used in response also
const char kValueStillDataQuality[] = "quality";
const char kValueStillDataExtension[] = "extension";
const char kValueStillDataVerbose[] = "verbose";
const char kValueStillDataForceCapture[] = "force_capture";
const char kValueStillDataUrl[] = "url";  // used in response only

//
//  Media Config Version
//
//  v0.75
//    -  The deviceid request is deleted and included in the info request.
const char kMediaConfigVersion[] = "v0.75";

//
// Error messages
//
const char kErrDataKeyMissing[] = "data key missing";
const char kErrInternalError[] = "Unknown Internal Error";
const char kErrClientOrRoomIdNotFound[] =
    "No Client/Room ID found in register command";
const char kErrRegisterClientOrRoomId[] =
    "Failed to register Client/Room ID in clientinfo";
const char kErrInvalidJsonMessage[] =
    "Failed to parse Json Message in send command";
const char kErrStreamerOccupied[] =
    "Streamer session is already in use by another user";
const char kErrMessageEmpy[] = "No json message in cmd send";
const char kErrUnknownCommandType[] = "Unknown Command Type";
const char kErrUnknownRequestType[] = "Unknown Request Type";
const char kErrUnknownProtocolMessage[] = "Unknown Protocol Message";
const char kErrRTCConfig[] = "Failed to get RTC Configuration";
const char kErrStillDataParse[] = "Failed to parse data of still parameters";

// delay of message to use for stream release
const int kStreamReleaseDelay = 1000;
const int kMaxChunkedFrames = 5;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// App Websocket only Channel
//
////////////////////////////////////////////////////////////////////////////////
AppWsClient::AppWsClient(StreamerProxy* proxy)
    : SignalingChannelHelper(proxy), num_chunked_frames_(0) {
    deviceid_inited_ = utils::GetHardwareDeviceId(&deviceid_);
    config_media_ = ConfigMediaSingleton::Instance();
    config_streamer_ = nullptr;
    if (deviceid_inited_)
        RTC_LOG(INFO) << "Using Device ID : " << deviceid_;
    else
        RTC_LOG(LS_ERROR) << "Failed to get device id";
}

void AppWsClient::RegisterWebSocketMessage(WebSocketMessage* server_instance) {
    websocket_message_ = server_instance;
}

void AppWsClient::RegisterConfigStreamer(ConfigStreamer* config_streamer) {
    config_streamer_ = config_streamer;
}

void AppWsClient::OnConnect(int sockid) {
    RTC_LOG(INFO) << "New WebSocket connnection id : " << sockid;
    // TODO: need to implement client context container per id
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
        RTC_LOG(LS_VERBOSE) << "JSON Parsing Success: " << message;
    } else {
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

    if (rtc::GetStringFromJsonObject(json_value, kKeyCmd, &cmd) == false) {
        RTC_LOG(LS_ERROR) << "Received unknown protocol message. " << message;
        SendEvent(sockid, EventError, kErrUnknownProtocolMessage);
        return true;
    }

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

        RTC_LOG(INFO) << "Room id: " << room_id << ", client id: " << client_id;
        if (app_client_.Register(sockid, room_id, client_id) == false) {
            RTC_LOG(LS_ERROR) << "Failed to set room_id/client_id :" << message;
            SendEvent(sockid, EventNotice, kErrStreamerOccupied);
            return true;
        };

        // Adding Session RTC config mapping,
        RTC_LOG(INFO) << "Add session rtc config mapping sockid: " << sockid
                      << ", client id: " << client_id;

        if (IsSignalingSessionActive() == false) {
            SessionConfig::Config config;
            session_config_.GetConfig(sockid, config);
            if (StartSignalingSession(client_id, config) == true) {
                RTC_LOG(INFO) << "New WebSocket Name: " << client_id;
            };
            return true;
        } else {
            RTC_LOG(LS_ERROR) << "Internal Error, The connection is terminated"
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
                    if (IsSignalingSessionActive() == true) {
                        RTC_LOG(INFO) << "Session is not Active : "
                                         "Stopping Signaling Session";
                        StopSignalingSession();
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
                rtc::GetStringFromJsonObject(json_data_value, kKeyDataCommand,
                                             &command);
                if (command.empty()) {
                    RTC_LOG(LS_ERROR)
                        << "Failed to get zoom command: " << message;
                    return true;
                }
                if (rtc::GetDoubleFromJsonObject(json_data_value, kValueDataX,
                                                 &cx) == false ||
                    rtc::GetDoubleFromJsonObject(json_data_value, kValueDataY,
                                                 &cy) == false) {
                    RTC_LOG(LS_ERROR)
                        << "Failed to get cx/cy position : " << message;
                    return true;
                }
                RTC_LOG(INFO) << "Zoom Command Type " << command
                              << ", Position " << cx << ", " << cy;
                if (command.compare(kValueCommandZoomIn) == 0) {
                    webrtc::MMALWrapper::Instance()->Zoom(
                        {.cmd = wstreamer::ZoomOptions::IN,
                         .center_x = cx,
                         .center_y = cy});
                } else if (command.compare(kValueCommandZoomOut) == 0) {
                    webrtc::MMALWrapper::Instance()->Zoom(
                        {.cmd = wstreamer::ZoomOptions::OUT});
                } else if (command.compare(kValueCommandZoomReset) == 0) {
                    webrtc::MMALWrapper::Instance()->Zoom(
                        {.cmd = wstreamer::ZoomOptions::RESET});
                } else if (command.compare(kValueCommandZoomMove) == 0) {
                    // FIXME: problem in movve command during ROI enabled
                    webrtc::MMALWrapper::Instance()->Zoom(
                        {.cmd = wstreamer::ZoomOptions::MOVE,
                         .center_x = cx,
                         .center_y = cy});
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
        std::string cmd_type, transaction;

        rtc::GetStringFromJsonObject(json_value, kKeyCmdType, &cmd_type);
        rtc::GetStringFromJsonObject(json_value, kKeyTransaction, &transaction);
        RTC_LOG(INFO) << "Request Ccmd: " << cmd_type
                      << ", Transaction: " << transaction;
        //
        // Device info request
        //
        if (cmd_type.compare(kValueTypepDeviceInfo) == 0) {
            int supported = 0, detected = 0;
            raspicamcontrol_get_camera(&supported, &detected);
            Json::Value response_data;
            response_data[kDataKeyDeviceId] = deviceid_;
            response_data[kDataKeyMcVersion] = kMediaConfigVersion;
            response_data[kDataKeyStillCapture] =
                config_media_->GetStillEnable();
            response_data[kDataKeyVideo43AspectRatio] =
                config_media_->GetResolution4_3();
            response_data[kDataKeyCameraEnabled] =
                supported > 0 && detected > 0 ? true : false;
            SendResponse(sockid, true, kValueTypepDeviceInfo, transaction,
                         response_data.toStyledString(), "");
            return true;
        }
        //
        // RTC Config request
        //
        else if (cmd_type.compare(kValueTypeRTCConfig) == 0) {
            // { cmd : response, type: rtcconfig, data: { ... }, result:
            // 'SUCCESS/FAILED', error: '...' }
            std::string json_rtcconfig;
            Json::Value response_data;
            // Json::StyledWriter json_writer;

            if (rtc::GetValueFromJsonObject(json_value, kKeyData,
                                            &response_data) == true) {
                // it's setting request when data exists in rtcconfig request,
                utils::RTCConfiguration rtc_config;  // for validation
                json_rtcconfig = response_data.toStyledString();
                std::string error_message;

                // just validate the json message of RTCConfiguration
                if (utils::RTCConfigFromJson(rtc_config, json_rtcconfig,
                                             error_message)) {
                    // success to validate json config and
                    // set josn string as session config
                    session_config_.SetRtcConfig(sockid, json_rtcconfig);
                    utils::PrintRTCConfig(rtc_config);

                    // Session RTC Configuration
                    // session_rtcconfig_.Set(sockid, json_rtcconfig);
                    SendResponse(sockid, true, kValueTypeRTCConfig, transaction,
                                 "", "");
                } else
                    SendResponse(sockid, false, kValueTypeRTCConfig,
                                 transaction, "", error_message);
                return true;
            } else {
                // loading json RTC config from configuration file
                if (config_streamer_->GetJsonRtcConfig(json_rtcconfig)) {
                    SendResponse(sockid, true, kValueTypeRTCConfig, transaction,
                                 json_rtcconfig, "");
                } else {
                    RTC_LOG(LS_ERROR) << "Failed to get JSON RTC Config";
                    SendResponse(sockid, false, kValueTypeRTCConfig,
                                 transaction, "", kErrRTCConfig);
                }
            }
            return true;
        }

        //
        // Config request
        //
        else if (cmd_type.compare(kValueTypeConfig) == 0) {
            // { cmd : request, type: config, data : { ... }  }
            std::string data, json_error, updated_config;

            if (rtc::GetStringFromJsonObject(json_value, kKeyData, &data) ==
                false) {
                // data key is not found;
                RTC_LOG(LS_ERROR) << "Failed to get data key";
                SendResponse(sockid, false, kValueTypeConfig, transaction, "",
                             kErrDataKeyMissing);
                return true;
            }

            //
            // Read Command
            //
            if (data.compare(kValueDataRead) == 0) {
                // sends the entire media_config to the client.
                std::string media_config;

                config_media_->ToJson(media_config);
                SendResponse(sockid, true, kValueTypeConfig, transaction,
                             media_config, "");
                return true;
            }
            //
            // Save Command
            //
            else if (data.compare(kValueDataSave) == 0) {
                if (config_media_->Save() == false) {
                    RTC_LOG(LS_ERROR) << "Failed to save media config";
                    SendResponse(sockid, false, kValueTypeConfig, transaction,
                                 "{}", "");
                    return true;
                } else {
                    // save successufl,
                    // and sends the entire media_config to the client.
                    std::string media_config;
                    config_media_->ToJson(media_config);
                    SendResponse(sockid, true, kValueTypeConfig, transaction,
                                 media_config, "");
                    return true;
                }
            }
            //
            // Reset Command
            //
            else if (data.compare(kValueDataReset) == 0) {
                // reset to default media configurations
                config_media_->Reset();

                // sends the entire media_config to the client.
                std::string media_config;

                config_media_->ToJson(media_config);
                SendResponse(sockid, true, kValueTypeConfig, transaction,
                             media_config, "");
                return true;
            }
            //
            // Apply Command
            //
            else if (data.compare(kValueDataApply) == 0) {
                if (IsSignalingSessionActive() == true) {
                    webrtc::MMALWrapper::Instance()->SetEncoderConfigParams();
                    if (webrtc::MMALWrapper::Instance()
                            ->ReinitEncoderInternal() == true) {
                        RTC_LOG(INFO) << "ReinitEncoderInternal Success";
                        // restart capture
                        webrtc::MMALWrapper::Instance()->StartCapture();
                        SendResponse(sockid, true, kValueTypeConfig,
                                     transaction, "", "");
                    } else {
                        RTC_LOG(LS_ERROR) << "Failed to ReinitEncoderInternal";
                        SendResponse(sockid, false, kValueTypeConfig,
                                     transaction, "", "");
                    }
                } else {
                    // do not neeed to init the entire video encoding
                    SendResponse(sockid, true, kValueTypeConfig, transaction,
                                 "", "");
                }
                return true;
            }

            if (config_media_->FromJson(data, &updated_config, json_error) ==
                false) {
                RTC_LOG(LS_ERROR) << "Failed to parse config data";
                SendResponse(sockid, false, kValueTypeConfig, transaction, "",
                             json_error);
            } else {
                RTC_LOG(INFO) << "Media Config : " << updated_config;
                SendResponse(sockid, true, kValueTypeConfig, transaction,
                             updated_config, "");
            }
            return true;
        }

        //
        // Still image capture request
        //
        else if (cmd_type.compare(kValueTypeStill) == 0) {
            // { cmd : request, type: still, data : { ... }  }
            Json::Value json_data_value;
            if (IsSignalingSessionActive() == true) {
                SendResponse(sockid, false, kValueTypeStill, transaction, "",
                             kErrStreamerOccupied);
                return true;
            }

            if (rtc::GetValueFromJsonObject(json_value, kKeyData,
                                            &json_data_value) == false) {
                RTC_LOG(LS_ERROR) << "Failed to get data key";
                SendResponse(sockid, false, kValueTypeStill, transaction, "",
                             kErrDataKeyMissing);
                return true;
            }

            if (config_media_->GetStillEnable() == false) {
                RTC_LOG(LS_ERROR) << "Failed to get data key";
                SendResponse(sockid, false, kValueTypeStill, transaction, "",
                             "still capturing is not enabled");
                return true;
            }

            wstreamer::StillOptions options;
            std::string captured_filename;
            int int_value;
            double double_value;
            std::string str_value;
            bool bool_value;

            if (rtc::GetIntFromJsonObject(json_data_value, kValueStillDataWidth,
                                          &int_value) == true) {
                // data key is not found;
                RTC_LOG(INFO) << "Still width: " << int_value;
                options.width = int_value;
            }
            if (rtc::GetIntFromJsonObject(json_data_value,
                                          kValueStillDataHeight,
                                          &int_value) == true) {
                // data key is not found;
                RTC_LOG(INFO) << "Still height: " << int_value;
                options.height = int_value;
            }
            if (rtc::GetIntFromJsonObject(json_data_value,
                                          kValueStillDataCameraNum,
                                          &int_value) == true) {
                // data key is not found;
                RTC_LOG(INFO) << "Still cameraNum: " << int_value;
                options.cameraNum = int_value;
            }
            if (rtc::GetStringFromJsonObject(json_data_value,
                                             kValueStillDataFilename,
                                             &str_value) == true) {
                // data key is not found;
                RTC_LOG(INFO) << "Still filename: " << str_value;
                options.filename = str_value;
            }
            if (rtc::GetDoubleFromJsonObject(json_data_value,
                                             kValueStillDataQuality,
                                             &double_value) == true) {
                // data key is not found;
                RTC_LOG(INFO) << "Still quality: " << double_value;
                options.quality = double_value;
            }
            if (rtc::GetStringFromJsonObject(json_data_value,
                                             kValueStillDataExtension,
                                             &str_value) == true) {
                // data key is not found;
                RTC_LOG(INFO) << "Still extension: " << str_value;
                options.extension = str_value;
            }
            if (rtc::GetBoolFromJsonObject(json_data_value,
                                           kValueStillDataVerbose,
                                           &bool_value) == true) {
                // data key is not found;
                RTC_LOG(INFO) << "Still verbose: " << bool_value;
                options.verbose = bool_value;
            }
            if (rtc::GetBoolFromJsonObject(json_data_value,
                                           kValueStillDataForceCapture,
                                           &bool_value) == true) {
                // data key is not found;
                RTC_LOG(INFO) << "Still forced capture: " << bool_value;
                options.force_capture = bool_value;
            }

            absl::Status status =
                webrtc::StillCapture::Instance()->GetLatestOrCapture(
                    options, &captured_filename);
            RTC_LOG(INFO) << "Still Capture Status : " << status.ToString();
            if (status.ok()) {
                Json::StyledWriter json_writer;
                Json::Value json_data;
                RTC_LOG(INFO) << "Still Captured status " << status.ToString()
                              << ", filename: " << captured_filename;
                json_data[kValueStillDataFilename] = captured_filename;
                json_data[kValueStillDataUrl] =
                    absl::StrCat("/still/", captured_filename);
                SendResponse(sockid, true, kValueTypeStill, transaction,
                             json_writer.write(json_data), "");
            } else {
                RTC_LOG(INFO) << "Failed to capture:  " << status.ToString();
                SendResponse(sockid, false, kValueTypeStill, transaction, "",
                             status.ToString());
            }
            return true;
        }
        RTC_LOG(LS_ERROR) << "Unknown Request type: " << cmd_type;
        SendEvent(sockid, EventError, kErrUnknownRequestType);
        return true;
    }
    SendEvent(sockid, EventError, kErrUnknownCommandType);
    return true;
}

void AppWsClient::OnDisconnect(int sockid) {
    RTC_LOG(INFO) << "WebSocket connnection id : " << sockid << " closed";

    // clear the session config
    session_config_.Remove(sockid);

    // Ignore if websocket id is not the registered websocket id.
    if (app_client_.Disconnect(sockid) == true) {
        app_client_.Reset();
        if (IsSignalingSessionActive() == true) {
            StopSignalingSession();
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
    int sockid;

    RTC_LOG(INFO) << __FUNCTION__;
    if (app_client_.GetSockId(peer_id, sockid) == true) {
        Json::StyledWriter json_writer;
        Json::Value json_message;

        json_message[kKeyCmd] = kValueCmdSend;
        json_message[kKeySendMessage] = message;
        websocket_message_->SendMessage(sockid,
                                        json_writer.write(json_message));
        return true;
    }
    return false;
}

void AppWsClient::ReportEvent(const int peer_id, bool drop_connection,
                              const std::string& message) {
    RTC_DCHECK(websocket_message_ != nullptr)
        << "WebSocket Server instance is nullptr";
    int sockid;

    RTC_LOG(INFO) << __FUNCTION__;

    if (app_client_.GetSockId(peer_id, sockid) == true) {
        if (drop_connection == true) {
            rtc::Thread::Current()->PostDelayed(
                RTC_FROM_HERE, kStreamReleaseDelay, this, 3400,
                new rtc::TypedMessageData<int>(sockid));

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
    int sockid = msg_data->data();

    //  Internally, there is no reason to keep the session anymore,
    //  so it terminates the session that is currently being held.
    RTC_LOG(INFO) << __FUNCTION__ << "Drop WebSocket Connection : " << sockid;
    websocket_message_->Close(sockid, 0, "");
    if (app_client_.Disconnect(sockid) == true) {
        app_client_.Reset();
        if (IsSignalingSessionActive() == true) {
            StopSignalingSession();
        };
    }
    return;
}

void AppWsClient::SendResponse(int sockid, bool success,
                               const std::string& type,
                               const std::string& transaction,
                               const std::string& data,
                               const std::string& error_mesg) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(websocket_message_ != nullptr)
        << "WebSocket Server instance is nullptr";
    std::string json_resp_mesg;
    Json::StyledWriter json_writer;
    Json::Value json_response;

    json_response[kKeyCmd] = kValueCmdResponse;
    json_response[kKeyCmdType] = type;
    json_response[kKeyData] = data;
    if (!transaction.empty()) json_response[kKeyTransaction] = transaction;
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
