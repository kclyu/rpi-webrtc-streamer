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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config_streamer.h"

#include <iostream>
#include <memory>
#include <string>

#include "config_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "utils.h"
#include "utils_pc_strings.h"

////////////////////////////////////////////////////////////////////////////////
//
// Default values
//
////////////////////////////////////////////////////////////////////////////////
static const char kDefaultStreamerConfig[] = "etc/webrtc_streamer.conf";

////////////////////////////////////////////////////////////////////////////////
//
// streamer config key name and constants values
//
////////////////////////////////////////////////////////////////////////////////

CONFIG_DEFINE(WebSocketEnable, websocket_enable, bool, true);
CONFIG_DEFINE(WebSocketPort, websocket_port, int, 8889);
CONFIG_DEFINE(DirectSocketPort, direct_socket_port, int, 8888);
CONFIG_DEFINE(DirectSocketEnable, direct_socket_enable, bool, false);
CONFIG_DEFINE(MediaConfig, media_config, std::string, "etc/media_config.conf");
CONFIG_DEFINE(MotionConfig, motion_config, std::string,
              "etc/motion_config.conf");
CONFIG_DEFINE(FieldTrials, fieldtrials, std::string, "");

CONFIG_DEFINE(DisableLogBuffering, disable_log_buffering, bool, true);
CONFIG_DEFINE(LibraryDebug, libwebsocket_debug, bool, false);

CONFIG_DEFINE(AudioEnable, audio_enable, bool, false);
CONFIG_DEFINE(VideoEnable, video_enable, bool, true);
CONFIG_DEFINE(SrtpEnable, srtp_enable, bool, true);

CONFIG_DEFINE(WebRoot, web_root, std::string, INSTALL_DIR "/web-root");
CONFIG_DEFINE(RWS_WS_URL, rws_ws_url, std::string, "/rws/ws");

////////////////////////////////////////////////////////////////////////////////
//
// Config Key
//
////////////////////////////////////////////////////////////////////////////////
// Ice Servers configuration keys
static const char kConfigIceTransportsType[] = "ice_transports_type";
static const char kConfigIceBundlePolicy[] = "bundle_policy";
static const char kConfigIceRtcpMuxPolicy[] = "rtcp_mux_policy";

static const char kConfigIceServerUrls[] = "ice_server_urls";
static const char kConfigIceServerInternalUrls[] = "ice_server_internal_urls";
static const char kConfigIceServerUsername[] = "ice_server_username";
static const char kConfigIceServerPassword[] = "ice_server_password";
static const char kConfigIceServerHostname[] = "ice_server_hostname";
static const char kConfigIceServerTlsCertPolicy[] =
    "ice_server_tls_cert_policy";
static const char kConfigIceServerTlsAlpnProtocols[] =
    "ice_server_tls_alpn_protocols";
static const char kConfigIceServerTlsEllipticCurves[] =
    "ice_server_tls_elliptic_curves";
static const int kMaxIceServers = 10;

////////////////////////////////////////////////////////////////////////////////
//
// config validation helper functions
//
////////////////////////////////////////////////////////////////////////////////
static bool validate__portnumber(int& port_number, int default_value) {
    if ((port_number < 1) || (port_number > 65535)) {
        RTC_LOG(LS_ERROR) << "Error in port value," << port_number
                          << " is not a valid port";
        RTC_LOG(LS_ERROR) << "Resetting to default value";
        port_number = default_value;
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// StreamerConfig, main config loading
//
////////////////////////////////////////////////////////////////////////////////
StreamerConfig::StreamerConfig(const std::string& config_file)
    : config_loaded_(false), config_file_(config_file), verbose_(false) {}

bool StreamerConfig::LoadConfig(bool verbose) {
    std::string file_path;
    verbose_ = verbose;

    // trying to check flag config_file is regular file
    if (utils::IsFile(config_file_) == false) {
        // there is no config file in command line flag
        // so, trying to load config from installation directory config path
        file_path = std::string(INSTALL_DIR) + "/" +
                    std::string(kDefaultStreamerConfig);

        if (utils::IsFile(file_path) == false) {
            std::cerr << "Failed to find config options:" << file_path
                      << std::endl;
            return false;
        };
    } else {
        file_path = config_file_;
    }

    config_.reset(new rtc::OptionsFile(file_path));
    if (config_->Load(verbose) == false) {
        std::cerr << "Failed to load config options:" << file_path << std::endl;
        return false;
    }

    config_file_ = file_path;
    config_dir_basename_ = utils::GetParentFolder(file_path);
    std::cout << "Using config file base path:"
              << ((config_dir_basename_.size() == 0) ? "CWD"
                                                     : config_dir_basename_)
              << std::endl;
    config_loaded_ = true;
    return true;
}

// DisableLogBuffering
bool StreamerConfig::GetDisableLogBuffering() {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_BOOL_WITH_RETURN(DisableLogBuffering);
}

// WebSocketEnable
bool StreamerConfig::GetWebSocketEnable() {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_BOOL_WITH_RETURN(WebSocketEnable);
}

// WebSocketPort
bool StreamerConfig::GetWebSocketPort(int& port) {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_INT_WITH_RETURN(WebSocketPort, port,
                                       validate__portnumber);
}

// LibwebsocketDebugEnable
bool StreamerConfig::GetLibwebsocketDebugEnable() {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_BOOL_WITH_RETURN(LibraryDebug);
}

// WebRootPath
bool StreamerConfig::GetWebRootPath(std::string& path) {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_STR_WITH_RETURN(WebRoot, path);
}

// RWS WS URL
bool StreamerConfig::GetRwsWsURL(std::string& ws_url) {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_STR_WITH_RETURN(RWS_WS_URL, ws_url);
}

// DirectSocketEnable
bool StreamerConfig::GetDirectSocketEnable() {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_BOOL_WITH_RETURN(DirectSocketEnable);
}

// DirectSocketPort
bool StreamerConfig::GetDirectSocketPort(int& port) {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_INT_WITH_RETURN(DirectSocketPort, port,
                                       validate__portnumber);
}

// SrtpEnable
bool StreamerConfig::GetSrtpEnable() {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_BOOL_WITH_RETURN(SrtpEnable);
}

// AudioEnable
bool StreamerConfig::GetAudioEnable() {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_BOOL_WITH_RETURN(AudioEnable);
}

// VideoEnable
bool StreamerConfig::GetVideoEnable() {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_BOOL_WITH_RETURN(VideoEnable);
}

// Get IceTransportsType
void StreamerConfig::GetIceTransportsType(
    webrtc::PeerConnectionInterface::RTCConfiguration& rtc_config) {
    RTC_DCHECK(config_loaded_ == true);
    std::string config_value;
    if (config_->GetStringValue(kConfigIceTransportsType, &config_value) ==
        true) {
        rtc_config.type = utils::ConfigToIceTransportsType(config_value);
    }
}

// Get IceBundlePolicy
void StreamerConfig::GetIceBundlePolicy(
    webrtc::PeerConnectionInterface::RTCConfiguration& rtc_config) {
    RTC_DCHECK(config_loaded_ == true);
    std::string config_value;
    if (config_->GetStringValue(kConfigIceBundlePolicy, &config_value) ==
        true) {
        rtc_config.bundle_policy = utils::ConfigToIceBundlePolicy(config_value);
    }
    // BundlePolicy does not exist in config, always it will return true
}

// Get IceRtcpMuxPolicy
void StreamerConfig::GetIceRtcpMuxPolicy(
    webrtc::PeerConnectionInterface::RTCConfiguration& rtc_config) {
    RTC_DCHECK(config_loaded_ == true);
    std::string config_value;
    if (config_->GetStringValue(kConfigIceRtcpMuxPolicy, &config_value) ==
        true) {
        rtc_config.rtcp_mux_policy =
            utils::ConfigToIceRtcpMuxPolicy(config_value);
    }
}

// IceServers
bool StreamerConfig::GetIceServers(
    webrtc::PeerConnectionInterface::RTCConfiguration& rtc_config,
    bool internal_config) {
    RTC_DCHECK(config_loaded_ == true);
    char config_key_buffer[512];
    std::string config_value;
    bool gen_config_exist = false;

#define LOAD_CONF_KEY(key, index)                                          \
    snprintf(config_key_buffer, sizeof(config_key_buffer), "%s_%d", key,   \
             index);                                                       \
    if (config_->GetStringValue(config_key_buffer, &config_value) == true) \
        gen_config_exist = true;                                           \
    else                                                                   \
        gen_config_exist = false;

    for (int config_index = 0; config_index < kMaxIceServers; config_index++) {
        LOAD_CONF_KEY(kConfigIceServerUrls, config_index);
        if (gen_config_exist) {
            webrtc::PeerConnectionInterface::IceServer ice_server;

            // ice servers
            ice_server.urls = utils::ConfigToIceUrls(config_value);
            LOAD_CONF_KEY(kConfigIceServerInternalUrls, config_index);
            if (internal_config) {
                if (gen_config_exist) {
                    // replace ice server url with internal config url
                    ice_server.urls = utils::ConfigToIceUrls(config_value);
                }
            }

            // username
            LOAD_CONF_KEY(kConfigIceServerUsername, config_index);
            if (gen_config_exist) ice_server.username = config_value;
            // password
            LOAD_CONF_KEY(kConfigIceServerPassword, config_index);
            if (gen_config_exist) ice_server.password = config_value;
            // hostname
            LOAD_CONF_KEY(kConfigIceServerHostname, config_index);
            if (gen_config_exist) ice_server.hostname = config_value;
            // Cert Policy
            LOAD_CONF_KEY(kConfigIceServerTlsCertPolicy, config_index);
            if (gen_config_exist)
                ice_server.tls_cert_policy =
                    utils::ConfigToIceTlsCertPolicy(config_value);
            // Alpn Protocols
            LOAD_CONF_KEY(kConfigIceServerTlsAlpnProtocols, config_index);
            if (gen_config_exist) {
                ice_server.tls_alpn_protocols =
                    utils::ConfigToVector(config_value);
            }
            // TlsEllipticCurves
            LOAD_CONF_KEY(kConfigIceServerTlsEllipticCurves, config_index);
            if (gen_config_exist) {
                ice_server.tls_elliptic_curves =
                    utils::ConfigToVector(config_value);
            }

            // Add new ice server information
            rtc_config.servers.push_back(ice_server);
        }
    }

    return (bool)rtc_config.servers.size();
}

// JSON RTCConfig
bool StreamerConfig::GetRTCConfig(std::string& json_rtcconfig) {
    webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;

    GetIceTransportsType(rtc_config);
    GetIceRtcpMuxPolicy(rtc_config);
    GetIceBundlePolicy(rtc_config);
    if (GetIceServers(rtc_config, false /* internal_config */) == false) {
        RTC_LOG(LS_ERROR) << "Internal Errror, failed to load ICE servers";
        return false;
    };

    Json::Value jsonPCConfig;
    Json::StyledWriter json_writer;

    for (const webrtc::PeerConnectionInterface::IceServer& server :
         rtc_config.servers) {
        if (!server.urls.empty()) {
            Json::Value ice_server;
            Json::Value json_urls = rtc::StringVectorToJsonArray(server.urls);
            ice_server["urls"] = json_urls;
            if (!server.username.empty())
                ice_server["username"] = server.username;
            if (!server.password.empty())
                ice_server["credential"] = server.password;
            jsonPCConfig["iceServers"].append(ice_server);
        }
    }

    // BundlePolicy
    std::string data =
        utils::BundlePolicyToString(rtc_config.bundle_policy, true);
    if (!data.compare(utils::kDefaultBundlePolicy))
        jsonPCConfig["bundlePolicy"] = data;

    // IceTransportType
    data = utils::IceTransportsTypeToString(rtc_config.type, true);
    if (!data.compare(utils::kDefaultIceTransportsType))
        jsonPCConfig["iceTransportPolicy"] = data;

    // RtcpMuxPolicy
    data = utils::RtcpMuxPolicyToString(rtc_config.rtcp_mux_policy, true);
    if (!data.compare(utils::kDefaultRtcpMuxPolicy))
        jsonPCConfig["rtcpMuxPolicy "] = data;

    json_rtcconfig = json_writer.write(jsonPCConfig);
    return true;
}

bool StreamerConfig::GetMediaConfigFilePath(std::string& conf) {
    RTC_DCHECK(config_loaded_ == true);
    // default media config value is "etc/media_config.conf"
    if (config_->GetStringValue(kConfigMediaConfig, &conf) == true) {
        conf = config_dir_basename_ + conf;
        config_loaded__MediaConfig = true;  // to suppress warning
        return true;
    }
    conf = config_dir_basename_ + kDefaultMediaConfig;
    return false;
}

bool StreamerConfig::GetMotionConfigFilePath(std::string& conf) {
    RTC_DCHECK(config_loaded_ == true);
    // default media config value is "etc/motion_config.conf"
    if (config_->GetStringValue(kConfigMotionConfig, &conf) == true) {
        conf = config_dir_basename_ + conf;
        config_loaded__MotionConfig = true;  // to supporess warning
        return true;
    }
    conf = config_dir_basename_ + kDefaultMediaConfig;
    RTC_LOG(LS_ERROR) << "Using Fallback Config :" << conf;
    return false;
}

// FieldTrials string for WebRTC native code package
bool StreamerConfig::GetFieldTrials(std::string& fieldtrials) {
    RTC_DCHECK(config_loaded_ == true);
    DEFINE_CONFIG_LOAD_STR_WITH_RETURN(FieldTrials, fieldtrials);
}

// Just validate the log path,
// if the log path does not found, it will return false
bool StreamerConfig::GetLogPath(std::string& log_path) {
    RTC_DCHECK(config_loaded_ == true);

    if (utils::GetFolder(log_path).size() == 0) {
        // log path is current working directory
        std::cerr << "Using message logging log directory in CWD\n";
        std::cerr << "Using CWD log is for only development support feature"
                  << " do not use deployment/packaging environment\n";
        return true;
    }

    // trying to check comamand flag log path is directory
    if (utils::IsFolder(log_path) == true) {
        return true;
    }

    // checking INSTALL_DIR log path
    std::string new_log_path = std::string(INSTALL_DIR) + "/log";
    if (utils::IsFolder(new_log_path) == false) {
        std::cerr << "Failed to find the log directory " << new_log_path
                  << "\n";
        return false;
    }

    log_path = new_log_path;
    return true;
}

const std::string StreamerConfig::GetConfigFilename() {
    RTC_DCHECK(config_loaded_ == true);
    return config_file_;
}

StreamerConfig::~StreamerConfig() {}
