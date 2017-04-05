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

#include <memory>
#include <string>
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/arraysize.h"

#include "streamer_config.h"


// port number 8888 is fiexed for direct socket
const uint16_t kDefaultWebSocketPort = 8889;
const uint16_t kDefaultDirectSocketPort = 8888;
const char kDefaultAppChannelConfig[] = "etc/app_channel.conf";

const char kConfigWebSocketEnable[] = "websocket_enable";
const char kConfigWebSocketPort[] = "websocket_port";
const char kConfigDirectSocketEnable[] = "direct_socket_enable";
const char kConfigDirectSocketPort[] = "direct_socket_port";
const char kConfigDTLSEnable[] = "dtls_enable";
const char kConfigAppChannelConfig[] = "app_channel_config";
const char kConfigStunServer[] = "stun_server";
const char kConfigTurnServer[] = "turn_server";


// Default values
const char kDefaultStunServer[] = "stun:stun.l.google.com:19302";
const bool kDefaultDTLSEnable = true;
// Not used
const char kTurnIceServer[] = "turn:turn.hostname:3478?transport=tcp";
const char kTurnUsername[] = "username";
const char kTurnPassword[] = "password";

////////////////////////////////////////////////////////////////////////////////
// StreamerConfig
////////////////////////////////////////////////////////////////////////////////

StreamerConfig::StreamerConfig(const std::string &config_file) 
    : config_loaded_(false), config_(config_file) {
    if( config_.Load() == false ) {
        LOG(LS_WARNING) << "Failed to load config options:" << config_file;
        return;
    };
    config_loaded_ = true;
}


bool StreamerConfig::GetWebSocketEnable() {
    std::string websocket_enabled;
    if( config_.GetStringValue(kConfigWebSocketEnable, &websocket_enabled ) == true ) {
        return websocket_enabled.compare("true") == 0;
    }
    // websocket will be enabled by default
    return true;
}

bool StreamerConfig::GetWebSocketPort(int& port) {
    if( config_.GetIntValue(kConfigWebSocketPort, &port ) == true ) {
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
    std::string direct_socket_enabled;
    // direct_socket will not be enabled by default
    if( config_loaded_ == false )  return false;
    if( config_.GetStringValue(kConfigDirectSocketEnable, &direct_socket_enabled ) == true ) {
        return direct_socket_enabled.compare("true") == 0;
    }
    return false;
}

bool StreamerConfig::GetDirectSocketPort(int& port) {
    // direct_socket will not be enabled by default
    if( config_loaded_ == false )  return false;
    if( config_.GetIntValue(kConfigDirectSocketPort, &port ) == true ) {
        if ((port < 1) || (port > 65535)) {
            LOG(LS_ERROR) << "Error in port value," << port << " is not a valid port";
            LOG(LS_ERROR) << "Resetting to default value";
            port = kDefaultDirectSocketPort;
            return true;
        };
        return true;
    }
    return false;
}

bool StreamerConfig::GetDTLSEnable() {
    // DTLS will be enabled by default
    if( config_loaded_ == false )  return true;
    std::string dtls_enabled;
    if( config_.GetStringValue(kConfigDTLSEnable, &dtls_enabled ) == true ) {
        return dtls_enabled.compare("true") == 0;
    }
    return true;
}


bool StreamerConfig::GetStunServer(std::string& server) {
    // default stun server is kDefaultStunServer
    if( config_.GetStringValue(kConfigStunServer, &server ) == true ) {
        return true;
    }
    server = kDefaultStunServer;
    return true;
}

bool StreamerConfig::GetTurnServer(std::string& server) {
    // there is no default value for turn server 
    if( config_.GetStringValue(kConfigTurnServer, &server ) == true ) {
        return true;
    }
    return false;
}


//
bool StreamerConfig::GetAppChannelConfig(std::string& conf) {
    // default app channel config value is "etc/app_channel.conf"
    if( config_.GetStringValue(kConfigAppChannelConfig, &conf ) == true ) {
        return true;
    }
    conf = kDefaultAppChannelConfig;
    return false;
}


StreamerConfig::~StreamerConfig() {
}


