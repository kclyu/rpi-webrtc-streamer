/*
Copyright (c) 2021, rpi-webrtc-streamer Lyu,KeunChang

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

#include "config_defs.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/strings/string_format.h"
#include "system_wrappers/include/field_trial.h"
#include "utils.h"
#include "utils_pc_strings.h"

////////////////////////////////////////////////////////////////////////////////
//
// Default values
//
////////////////////////////////////////////////////////////////////////////////
static const char kDefaultConfigStreamer[] = "etc/webrtc_streamer.conf";

////////////////////////////////////////////////////////////////////////////////
//
// Ice Config Key
//
////////////////////////////////////////////////////////////////////////////////
// Ice Servers configuration keys
constexpr char kConfigIceTransportsType[] = "ice_transports_type";
constexpr char kConfigIceBundlePolicy[] = "bundle_policy";
constexpr char kConfigIceRtcpMuxPolicy[] = "rtcp_mux_policy";

constexpr char kConfigIceServerUrls[] = "ice_server_urls";
constexpr char kConfigIceServerUsername[] = "ice_server_username";
constexpr char kConfigIceServerPassword[] = "ice_server_password";
constexpr char kConfigIceServerHostname[] = "ice_server_hostname";
constexpr char kConfigIceServerTlsCertPolicy[] = "ice_server_tls_cert_policy";
constexpr char kConfigIceServerTlsAlpnProtocols[] =
    "ice_server_tls_alpn_protocols";
constexpr char kConfigIceServerTlsEllipticCurves[] =
    "ice_server_tls_elliptic_curves";
constexpr int kMaxIceServers = 10;

////////////////////////////////////////////////////////////////////////////////
//
// config validation helper functions
//
////////////////////////////////////////////////////////////////////////////////

DECLARE_METHOD_VALIDATOR(ConfigStreamer, websocket_port, int) {
    if (websocket_port < 0 || websocket_port > 65535) {
        std::cerr << "Error in port value," << websocket_port
                  << " is not a valid port\n";
        RTC_LOG(LS_ERROR) << "Resetting to default value\n";
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigStreamer, direct_socket_port, int) {
    if (direct_socket_port < 0 || direct_socket_port > 65535) {
        std::cerr << "Error in port value," << direct_socket_port
                  << " is not a valid port\n";
        std::cerr << "Resetting to default value\n";
        return false;
    }
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigStreamer, media_config, std::string) {
    return utils::IsFile(media_config);
}

DECLARE_METHOD_VALIDATOR(ConfigStreamer, motion_config, std::string) {
    return utils::IsFile(motion_config);
}

DECLARE_METHOD_VALIDATOR(ConfigStreamer, web_root, std::string) {
    return utils::IsFolder(web_root);
}

DECLARE_METHOD_VALIDATOR(ConfigStreamer, rws_ws_url, std::string) {
    // Nothing to validate
    return true;
}

DECLARE_METHOD_VALIDATOR(ConfigStreamer, fieldtrials, std::string) {
    return webrtc::field_trial::FieldTrialsStringIsValid(fieldtrials.c_str());
}

////////////////////////////////////////////////////////////////////////////////
//
// ConfigStreamer, Getter methods
//
////////////////////////////////////////////////////////////////////////////////

#define _CR(name, config_var, config_remote_access, config_type, \
            default_value)                                       \
    config_type ConfigStreamer::Get##name(void) const { return config_var; }

#define _CR_B _CR
#define _CR_I _CR

#include "def/config_streamer.def"

////////////////////////////////////////////////////////////////////////////////
//
// ConfigStreamer
//
////////////////////////////////////////////////////////////////////////////////
ConfigStreamer::ConfigStreamer(const std::string& config_file, bool dump_config)
    : config_loaded_(false),
      config_file_(config_file),
      dump_config_(dump_config) {
    // trying to check flag config_file is regular file
    if (utils::IsFile(config_file_) == false) {
        // there is no config file in command line flag
        // so, trying to load config from installation directory config path
        config_file_ = std::string(INSTALL_DIR) + "/" +
                       std::string(kDefaultConfigStreamer);

        if (utils::IsFile(config_file_) == false) {
            std::cerr << "Failed to find config file :" << config_file_ << "\n";
        };
    }

    config_dir_basename_ = utils::GetParentFolder(config_file_);
    std::cout << "Using config file base path:"
              << ((config_dir_basename_.size() == 0) ? "CWD"
                                                     : config_dir_basename_)
              << "\n";
}

bool ConfigStreamer::Load() {
    options_file_.reset(new rtc::OptionsFile(config_file_));
    if (options_file_->Load() == false) {
        std::cerr << "Failed to load config options:" << config_file_ << "\n";
        return false;
    }

    // Inititialize the config variables with default value
#define _CR(name, config_var, config_remote_access, config_type, \
            default_value)                                       \
    config_var = default_value;                                  \
    isloaded__##config_var = false;

#define _CR_B _CR
#define _CR_I _CR

#include "def/config_streamer.def"

#define _CR(name, config_var, config_remote_access, config_type,             \
            default_value)                                                   \
    {                                                                        \
        std::string config_value;                                            \
        if (options_file_->GetStringValue(#config_var, &config_value) ==     \
            true) {                                                          \
            if (validate_value__##config_var(config_value, default_value) == \
                true) {                                                      \
                isloaded__##config_var = true;                               \
                config_var = config_value;                                   \
            } else {                                                         \
                std::cerr << "Config " << #config_var                        \
                          << " error in value : " << config_value << "\n";   \
            };                                                               \
        }                                                                    \
    }

#define _CR_B(name, config_var, config_remote_access, config_type,         \
              default_value)                                               \
    {                                                                      \
        std::string config_value;                                          \
        if (options_file_->GetStringValue(#config_var, &config_value) ==   \
            true) {                                                        \
            if (config_value.compare("true") == 0)                         \
                config_var = true;                                         \
            else if (config_value.compare("false") == 0)                   \
                config_var = false;                                        \
            else {                                                         \
                std::cerr << "Config " << #config_var                      \
                          << " error in value : " << config_value << "\n"; \
            }                                                              \
        };                                                                 \
    }

#define _CR_I(name, config_var, config_remote_access, config_type,            \
              default_value)                                                  \
    {                                                                         \
        int config_value;                                                     \
        if (options_file_->GetIntValue(#config_var, &config_value) == true) { \
            if (validate_value__##config_var(config_value, default_value) ==  \
                true) {                                                       \
                isloaded__##config_var = true;                                \
                config_var = config_value;                                    \
            } else {                                                          \
                std::cerr << "Config " << #config_var                         \
                          << " error in value : " << config_value << "\n";    \
            };                                                                \
        }                                                                     \
    }

#include "def/config_streamer.def"

    config_loaded_ = true;
    if (dump_config_) DumpConfig();
    return true;
}

// loading RTCConfiguration from config file
bool ConfigStreamer::GetRtcConfig(utils::RTCConfiguration& config) {
    RTC_DCHECK(config_loaded_ == true);

    std::string config_value;
    if (options_file_->GetStringValue(kConfigIceTransportsType,
                                      &config_value) == true) {
        config.type = utils::StrToIceTransportsType(config_value);
    }
    if (options_file_->GetStringValue(kConfigIceBundlePolicy, &config_value) ==
        true) {
        config.bundle_policy = utils::StrToBundlePolicy(config_value);
    }
    if (options_file_->GetStringValue(kConfigIceRtcpMuxPolicy, &config_value) ==
        true) {
        config.rtcp_mux_policy = utils::StrToRtcpMuxPolicy(config_value);
    }

    for (int config_index = 0; config_index < kMaxIceServers; config_index++) {
        if (options_file_->GetStringValue(
                rtc::StringFormat("%s_%d", kConfigIceServerUrls, config_index),
                &config_value) == true) {
            webrtc::PeerConnectionInterface::IceServer ice_server;
            // ice servers
            ice_server.urls = utils::StrToIceUrls(config_value);

            // IceServerUsername
            if (options_file_->GetStringValue(
                    rtc::StringFormat("%s_%d", kConfigIceServerUsername,
                                      config_index),
                    &config_value)) {
                ice_server.username = config_value;
            }
            // IceServerPassword
            if (options_file_->GetStringValue(
                    rtc::StringFormat("%s_%d", kConfigIceServerPassword,
                                      config_index),
                    &config_value)) {
                ice_server.password = config_value;
            }
            // IceServerHostname
            if (options_file_->GetStringValue(
                    rtc::StringFormat("%s_%d", kConfigIceServerHostname,
                                      config_index),
                    &config_value)) {
                ice_server.hostname = config_value;
            }
            // IceServerCertPolicy
            if (options_file_->GetStringValue(
                    rtc::StringFormat("%s_%d", kConfigIceServerTlsCertPolicy,
                                      config_index),
                    &config_value)) {
                ice_server.tls_cert_policy =
                    utils::StrToTlsCertPolicy(config_value);
            }
            // IceServerAlpnProtocols
            if (options_file_->GetStringValue(
                    rtc::StringFormat("%s_%d", kConfigIceServerTlsAlpnProtocols,
                                      config_index),
                    &config_value)) {
                ice_server.tls_alpn_protocols =
                    utils::StrToVector(config_value);
            }
            // TlsEllipticCurves
            if (options_file_->GetStringValue(
                    rtc::StringFormat("%s_%d",
                                      kConfigIceServerTlsEllipticCurves,
                                      config_index),
                    &config_value)) {
                ice_server.tls_elliptic_curves =
                    utils::StrToVector(config_value);
            }
            // Add new ice server information
            config.servers.push_back(ice_server);
        }
    }
    // return size of iceServers
    return config.servers.size();
}

// JSON RTCConfig
bool ConfigStreamer::GetJsonRtcConfig(std::string& json_rtcconfig) {
    utils::RTCConfiguration rtc_config;
    Json::Value jsonPCConfig;
    Json::StyledWriter json_writer;

    GetRtcConfig(rtc_config);  // load rtc configuration from config
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

std::string ConfigStreamer::GetMediaConfigFile() {
    RTC_DCHECK(config_loaded_ == true);
    return config_dir_basename_ + GetMediaConfig();
}

std::string ConfigStreamer::GetMotionConfigFile() {
    RTC_DCHECK(config_loaded_ == true);
    return config_dir_basename_ + GetMotionConfig();
}

// Just validate the log path,
// if the log path does not found, it will return false
bool ConfigStreamer::GetLogPath(std::string& log_path) {
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

void ConfigStreamer::DumpConfig(void) {
    RTC_LOG(INFO) << "Config Dump : "
                  << " Filename : " << config_file_;
    RTC_LOG(INFO) << "Config Rows : ";

#define _CR(name, config_var, config_remote_access, config_type,               \
            default_value)                                                     \
    {                                                                          \
        const char* loaded_str = (isloaded__##config_var) ? "*" : " ";         \
        RTC_LOG(INFO) << "Config: " << loaded_str << "\"" << #config_var       \
                      << "\": " << config_var << " -- (type: " << #config_type \
                      << ", default: " << default_value << ")";                \
    }

#define _CR_B(name, config_var, config_remote_access, config_type,           \
              default_value)                                                 \
    {                                                                        \
        const char* loaded_str = (isloaded__##config_var) ? "*" : " ";       \
        const char* bool_str = (config_var) ? "true" : "false";              \
        const char* bool_default_str = (default_value) ? "true" : "false";   \
        RTC_LOG(INFO) << "Config: " << loaded_str << "\"" << #config_var     \
                      << "\": " << bool_str << " -- (type: " << #config_type \
                      << ", default: " << bool_default_str << ")";           \
    }
#define _CR_I _CR

#include "def/config_streamer.def"
}

ConfigStreamer::~ConfigStreamer() {}
