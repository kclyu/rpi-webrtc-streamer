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
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/fileutils.h"

#include "streamer_config.h"

// port number 8888 is fiexed for direct socket
const uint16_t kDefaultWebSocketPort = 8889;
const uint16_t kDefaultDirectSocketPort = 8888;
const char kDefaultStreamerConfig[] = "etc/webrtc_streamer.conf";
const char kDefaultAppChannelConfig[] = "etc/app_channel.conf";
const char kDefaultMediaConfig[] = "etc/media_config.conf";

const char kConfigWebSocketEnable[] = "websocket_enable";
const char kConfigWebSocketPort[] = "websocket_port";
const char kConfigDirectSocketEnable[] = "direct_socket_enable";
const char kConfigDirectSocketPort[] = "direct_socket_port";
const char kConfigAppChannelConfig[] = "app_channel_config";
const char kConfigMediaConfig[] = "media_config";
const char kConfigStunServer[] = "stun_server";
const char kConfigTurnServer[] = "turn_server";
const char kConfigDisableLogBuffering[] = "disable_log_buffering";

// Default values
const char kDefaultStunServer[] = "stun:stun.l.google.com:19302";

// Not used
const char kTurnIceServer[] = "turn:turn.hostname:3478?transport=tcp";
const char kTurnUsername[] = "username";
const char kTurnPassword[] = "password";

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
            std::cerr << "Failed to find config options:" << path.pathname() << "\n";
            return false;
        };
    };

    config_.reset(new rtc::OptionsFile(path.pathname()));
    if( config_->Load() == false ) {
        std::cerr  << "Failed to load config options:" << path.pathname() << "\n";
        return false;
    }

    config_file_ = path.pathname();
    config_dir_basename_ = path.parent_folder();
    std::cout << "Using config file base path:" << 
        ((config_dir_basename_.size() == 0)?"CWD":config_dir_basename_) << "\n";
    config_loaded_ = true;
    return true;
}


bool StreamerConfig::GetDisableLogBuffering() {
    RTC_CHECK( config_loaded_ == true );
    std::string disable_log_buffering;
    if( config_->GetStringValue(kConfigDisableLogBuffering, &disable_log_buffering ) == true ) {
        return disable_log_buffering.compare("true") == 0;
    }
    // default value for disable log buffering is true
    return true;
}

bool StreamerConfig::GetWebSocketEnable() {
    RTC_CHECK( config_loaded_ == true );
    std::string websocket_enabled;
    if( config_->GetStringValue(kConfigWebSocketEnable, &websocket_enabled ) == true ) {
        return websocket_enabled.compare("true") == 0;
    }
    // websocket will be enabled by default
    return true;
}

bool StreamerConfig::GetWebSocketPort(int& port) {
    RTC_CHECK( config_loaded_ == true );
    if( config_->GetIntValue(kConfigWebSocketPort, &port ) == true ) {
        if ((port < 1) || (port > 65535)) {
            LOG(LS_ERROR) << "Error in port value," << port << " is not a valid port";
            LOG(LS_ERROR) << "Resetting to default value";
            port = kDefaultWebSocketPort;
        }
        return true;
    }
    // websocket will be enabled by default
    port = kDefaultWebSocketPort;
    return true;
}

bool StreamerConfig::GetDirectSocketEnable() {
    RTC_CHECK( config_loaded_ == true );
    std::string direct_socket_enabled;
    // direct_socket will not be enabled by default
    if( config_loaded_ == false )  return false;
    if( config_->GetStringValue(kConfigDirectSocketEnable, 
                &direct_socket_enabled ) == true ) {
        return direct_socket_enabled.compare("true") == 0;
    }
    return false;
}

bool StreamerConfig::GetDirectSocketPort(int& port) {
    RTC_CHECK( config_loaded_ == true );
    // direct_socket will not be enabled by default
    if( config_loaded_ == false )  return false;
    if( config_->GetIntValue(kConfigDirectSocketPort, &port ) == true ) {
        if ((port < 1) || (port > 65535)) {
            LOG(LS_ERROR) << "Error in port value," 
                << port << " is not a valid port";
            LOG(LS_ERROR) << "Resetting to default value";
            port = kDefaultDirectSocketPort;
            return true;
        };
        return true;
    }
    return false;
}

bool StreamerConfig::GetStunServer(std::string& server) {
    RTC_CHECK( config_loaded_ == true );
    // default stun server is kDefaultStunServer
    if( config_->GetStringValue(kConfigStunServer, &server ) == true ) {
        return true;
    }
    server = kDefaultStunServer;
    return true;
}

bool StreamerConfig::GetTurnServer(std::string& server) {
    RTC_CHECK( config_loaded_ == true );
    // there is no default value for turn server 
    if( config_->GetStringValue(kConfigTurnServer, &server ) == true ) {
        return true;
    }
    return false;
}

//
bool StreamerConfig::GetAppChannelConfig(std::string& conf) {
    RTC_CHECK( config_loaded_ == true );
    // default app channel config value is "etc/app_channel.conf"
    if( config_->GetStringValue(kConfigAppChannelConfig, &conf ) == true ) {
        conf = config_dir_basename_ + conf;
        return true;
    }
    conf = config_dir_basename_ + kDefaultAppChannelConfig;
    return false;
}

//
bool StreamerConfig::GetMediaConfig(std::string& conf) {
    RTC_CHECK( config_loaded_ == true );
    // default media config value is "etc/media_config.conf"
    if( config_->GetStringValue(kConfigMediaConfig, &conf ) == true ) {
        conf = config_dir_basename_ + conf;
        return true;
    }
    conf = config_dir_basename_ + kDefaultMediaConfig;
    return false;
}

// Just validate the log path, 
// if the log path does not found, it will return false
bool StreamerConfig::GetLogPath(std::string& log_path) {
    RTC_CHECK( config_loaded_ == true );
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
    RTC_CHECK( config_loaded_ == true );
    return config_file_;
}


StreamerConfig::~StreamerConfig() {
}


