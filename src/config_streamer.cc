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

#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/fileutils.h"

#include "config_streamer.h"

////////////////////////////////////////////////////////////////////////////////
// Default Values
////////////////////////////////////////////////////////////////////////////////
static const uint16_t kDefaultWebSocketPort = 8889;
        // port number 8888 is fiexed for direct socket
static const uint16_t kDefaultDirectSocketPort = 8888; 
static const char kDefaultStreamerConfig[] = "etc/webrtc_streamer.conf";
static const char kDefaultMediaConfig[] = "etc/media_config.conf";
static const char kDefaultMotionConfig[] = "etc/motion_config.conf";

static const char kDefaultWebRoot[] = INSTALL_DIR "/web-root";
static const char kDefaultRoomId[] = "123456789";

// Stun
static const char kDefaultStunServer[] = "stun:stun.l.google.com:19302";
static const char kDefaultRWS_WS_URL[] = "/rws/ws";

////////////////////////////////////////////////////////////////////////////////
// Config Key
////////////////////////////////////////////////////////////////////////////////
static const char kConfigWebSocketEnable[] = "websocket_enable";
static const char kConfigWebSocketPort[] = "websocket_port";
static const char kConfigDirectSocketEnable[] = "direct_socket_enable";
static const char kConfigDirectSocketPort[] = "direct_socket_port";
static const char kConfigMediaConfig[] = "media_config";
static const char kConfigMotionConfig[] = "motion_config";
static const char kConfigDisableLogBuffering[] = "disable_log_buffering";
static const char kConfigRWS_WS_URL[] = "rws_ws_url";
static const char kConfigAdditionalWSRule[] = "additional_ws_rule";
static const char kConfigRoomIdEnable[] = "room_id_enable";
static const char kConfigRoomId[] = "room_id";

// STUN and TURN config
static const char kConfigStunServer[] = "stun_server";

static const char kConfigTurnServer[] = "turn_server";
static const char kConfigTurnUsername[] = "turn_username";
static const char kConfigTurnCredential[] = "turn_credential";

static const char kServerListDelimiter=',';
static const char kStunPrefix[] = "stun:";
static const char kTurnPrefix[] = "turn:";

// WebSocket Config
static const char kConfigWebRoot[] = "web_root";
static const char kConfigLibraryDebug[] = "libwebsocket_debug";

#define CONFIG_RETURN_BOOL_WITH_DEFAULT(key,default_value) \
        { \
            std::string flag_value; \
            if( config_->GetStringValue(key, &flag_value ) == true){ \
                if(flag_value.compare("true") == 0) return true; \
                else if (flag_value.compare("false") == 0) return false; \
                else { \
                    RTC_LOG(INFO) << "Default Config \"" <<  key \
                        << "\" value is not valid" << flag_value; \
                    return default_value; \
                };  \
            }; \
            return default_value; \
        };

#define CONFIG_RETURN_INT_WITH_DEFAULT(key,value,validate_function,default_value) \
        { \
            if( config_->GetIntValue(key, &value ) == true){ \
                return validate_function(value, default_value); \
            } \
            value = default_value; \
            return false; \
        };

#define CONFIG_RETURN_STR_WITH_DEFAULT(key,value, default_value) \
        { \
            if( config_->GetStringValue(key, &value ) == true){ \
                return true; \
            }; \
            value = default_value; \
            return false; \
        };

///////////////////////////////////////////////////////////////////////////////////////////
//
// config helper function
//
///////////////////////////////////////////////////////////////////////////////////////////
static bool validate__portnumber(int &port_number, int default_value ) {
    if ((port_number < 1) || (port_number > 65535)) {
            RTC_LOG(LS_ERROR) << "Error in port value," << port_number << " is not a valid port";
            RTC_LOG(LS_ERROR) << "Resetting to default value";
            port_number = default_value;
            return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// StreamerConfig
//
////////////////////////////////////////////////////////////////////////////////
StreamerConfig::StreamerConfig(const std::string &config_file) 
    : config_loaded_(false), config_file_(config_file) {}


bool StreamerConfig::LoadConfig()  {
    rtc::Pathname path;
    path.SetPathname( config_file_ );

    // trying to check flag config_file is regular file
    if( rtc::Filesystem::IsFile(path) == false)  {
        // there is no config file in command line flag 
        // so, trying to load config from installation directory config path
        path.SetPathname( std::string(INSTALL_DIR) 
                + "/"  + std::string(kDefaultStreamerConfig) );
        if( rtc::Filesystem::IsFile(path) == false)  {
            std::cerr << "Failed to find config options:" << path.pathname() << std::endl;
            return false;
        };
    };

    config_.reset(new rtc::OptionsFile(path.pathname()));
    if( config_->Load() == false ) {
        std::cerr  << "Failed to load config options:" << path.pathname() << std::endl;
        return false;
    }

    config_file_ = path.pathname();
    config_dir_basename_ = path.parent_folder();
    std::cout << "Using config file base path:" << 
        ((config_dir_basename_.size() == 0)?"CWD":config_dir_basename_) << std::endl;
    config_loaded_ = true;
    return true;
}


// DisableLogBuffering
bool StreamerConfig::GetDisableLogBuffering() {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_BOOL_WITH_DEFAULT(kConfigDisableLogBuffering, true );
}

// WebSocketEnable
bool StreamerConfig::GetWebSocketEnable() {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_BOOL_WITH_DEFAULT(kConfigWebSocketEnable, true );
}

// WebSocketPort
bool StreamerConfig::GetWebSocketPort(int& port) {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_INT_WITH_DEFAULT(kConfigWebSocketPort, port, 
            validate__portnumber,  kDefaultWebSocketPort);
}

// LibwebsocketDebugEnable
bool StreamerConfig::GetLibwebsocketDebugEnable() {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_BOOL_WITH_DEFAULT(kConfigLibraryDebug, false );
}

// WebRootPath
bool StreamerConfig::GetWebRootPath(std::string& path) {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_STR_WITH_DEFAULT(kConfigWebRoot, path, kDefaultWebRoot );
}

// RWS WS URL
bool StreamerConfig::GetRwsWsURL(std::string& ws_url) {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_STR_WITH_DEFAULT(kConfigRWS_WS_URL, ws_url, 
            kDefaultRWS_WS_URL );
}

// AdditionalWSRule
bool StreamerConfig::GetAdditionalWSRule(std::string& rule) {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_STR_WITH_DEFAULT(kConfigAdditionalWSRule, rule, "" );
}

// DirectSocketEnable
bool StreamerConfig::GetDirectSocketEnable() {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_BOOL_WITH_DEFAULT(kConfigDirectSocketEnable, false );
}

// DirectSocketPort
bool StreamerConfig::GetDirectSocketPort(int& port) {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_INT_WITH_DEFAULT(kConfigDirectSocketPort, port, 
            validate__portnumber,  kDefaultDirectSocketPort);
}

// RoomIdEnable
bool StreamerConfig::GetRoomIdEnable() {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_BOOL_WITH_DEFAULT(kConfigRoomIdEnable, false );
}

// RoomId
bool StreamerConfig::GetRoomId(std::string& room_id) {
    RTC_DCHECK( config_loaded_ == true );
    CONFIG_RETURN_STR_WITH_DEFAULT(kConfigRoomId, room_id, kDefaultRoomId);
}

// StunServer
bool StreamerConfig::GetStunServer(webrtc::PeerConnectionInterface::IceServer &server){
    RTC_DCHECK( config_loaded_ == true );
    std::string server_list;
    std::string username, credential;

    if( config_->GetStringValue(kConfigStunServer, &server_list ) == false ) {
        // server list does not in the config file
        // default stun server is kDefaultStunServer
        server.urls.push_back( kDefaultStunServer );
        return true;
    }

    std::stringstream ss(server_list);
    std::string token;
    int count=0;

    while( getline(ss, token, kServerListDelimiter) ) {
        // only compare "stun:" prefix
        if( token.compare(0, 5 /* kStunPrefix len */, kStunPrefix) == 0 )  {
            count++;
            server.urls.push_back(token);
        }
        else {
            RTC_LOG(LS_ERROR) << "Ignored stun server url : " << token;
        }
    }

    return (count?true:false);
}


// TurnServer
bool StreamerConfig::GetTurnServer(webrtc::PeerConnectionInterface::IceServer &server){
    RTC_DCHECK( config_loaded_ == true );
    std::string server_list;
    std::string username, credential;

    if( config_->GetStringValue(kConfigTurnServer, &server_list ) == false ) {
        // there is no default turn server 
        return false;
    }

    if( config_->GetStringValue(kConfigTurnUsername, &username ) == true ) {
        server.username = username;
        if( config_->GetStringValue(kConfigTurnCredential, &credential ) == true ) {
            server.password = credential;
        };
    };

    std::stringstream ss(server_list);
    std::string token;
    int count=0;

    while( getline(ss, token, kServerListDelimiter) ) {
        // only compare "turn:" prefix
        if( token.compare(0, 5 /* kTurnPrefix len*/, kTurnPrefix) == 0 )  {
            count++;
            server.urls.push_back(token);
            RTC_LOG(LS_ERROR) << "Adding Turn server url : " << token;
        }
        else {
            RTC_LOG(LS_ERROR) << "Ignored Turn server url : " << token;
        }
    }

    return (count?true:false);
}

bool StreamerConfig::GetMediaConfig(std::string& conf) {
    RTC_DCHECK( config_loaded_ == true );
    // default media config value is "etc/media_config.conf"
    if( config_->GetStringValue(kConfigMediaConfig, &conf ) == true ) {
        conf = config_dir_basename_ + conf;
        return true;
    }
    conf = config_dir_basename_ + kDefaultMediaConfig;
    return false;
}

bool StreamerConfig::GetMotionConfig(std::string& conf) {
    RTC_DCHECK( config_loaded_ == true );
    // default media config value is "etc/motion_config.conf"
    if( config_->GetStringValue(kConfigMotionConfig, &conf ) == true ) {
        conf = config_dir_basename_ + conf;
        return true;
    }
    conf = config_dir_basename_ + kDefaultMediaConfig;
    return false;
}

// Just validate the log path, 
// if the log path does not found, it will return false
bool StreamerConfig::GetLogPath(std::string& log_path) {
    RTC_DCHECK( config_loaded_ == true );
    rtc::Pathname path;
    path.SetPathname( log_path );

    // trying to check log path is directory 
    if( path.folder().size() == 0 &&  rtc::Filesystem::IsFolder(path) == true )  {
        // log path is current working directory 
        std::cerr << "Using message logging log directory in CWD\n";
        std::cerr << "Using CWD log is for only development support feature"
            << " do not use deployment/packaging environment\n";
        return true;
    }

    // trying to check comamand flag log path is directory 
    if( rtc::Filesystem::IsFolder(path) == true )  {
        return true;
    }

    // checking INSTALL_DIR log path
    path.SetPathname( std::string(INSTALL_DIR) + "/log" );
    if( rtc::Filesystem::IsFolder(path) == false )  {
        std::cerr << "Failed to find the log directory " 
            << path.pathname() << "\n";
        return false;
    }

    log_path = path.pathname();
    return true;
}

const std::string StreamerConfig::GetConfigFilename() {
    RTC_DCHECK( config_loaded_ == true );
    return config_file_;
}


StreamerConfig::~StreamerConfig() {
}


