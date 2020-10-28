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

#include "utils_pc_config.h"

#include <map>
#include <string>

#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/strings/string_format.h"
#include "utils_pc_strings.h"

constexpr char kConfigDelimiter = ',';
constexpr char kDefaultStunServer[] = "stun:stun.l.google.com:19302";

const char* kValidIceServiceTypes[] = {"stun", "stuns", "turn", "turns"};
const char kDeliimiterSchemeAndPort = ':';
const char kDeliimiterTransport = '?';
const char kSpanNumericIpAddress[] = ".1234567890:";  // do not check IPv6
enum ServiceSchemeType {
    STUN = 0,  // Indicates a STUN server.
    STUNS,     // Indicates a STUN server used with a TLS session.
    TURN,      // Indicates a TURN server
    TURNS,     // Indicates a TURN server used with a TLS session.
    INVALID,   // Unknown.
};

constexpr char kBundlePolicy[] = "bundlePolicy";
constexpr char kIceServers[] = "iceServers";
constexpr char kIceTransportPolicy[] = "iceTransportPolicy";
constexpr char kRtcpMuxPolicy[] = "rtcpMuxPolicy";
constexpr char kIceServersUrls[] = "urls";
constexpr char kIceServersUsername[] = "username";
constexpr char kIceServersPassword[] = "password";
constexpr char kIceServersCredential[] = "credential";

namespace utils {

bool RTCConfigFromJson(utils::RTCConfiguration& config,
                       const std::string& json_string,
                       std::string& error_message) {
    Json::Reader reader;
    Json::Value json_value;
    std::string value;
    if (!reader.parse(json_string, json_value)) {
        error_message = "Failed to parse json string";
        return false;
    }
    // bundlePolicy
    if (rtc::GetStringFromJsonObject(json_value, kBundlePolicy, &value) ==
        true) {
        config.bundle_policy = utils::StrToBundlePolicy(value);
    }
    // iceTransportType
    if (rtc::GetStringFromJsonObject(json_value, kIceTransportPolicy, &value) ==
        true) {
        config.type = utils::StrToIceTransportsType(value);
    }
    // rtcpMuxPolicy
    if (rtc::GetStringFromJsonObject(json_value, kRtcpMuxPolicy, &value) ==
        true) {
        config.rtcp_mux_policy = utils::StrToRtcpMuxPolicy(value);
    }

    // iceServers
    Json::Value json_v_iceservers;
    std::vector<Json::Value> iceservers;
    if (rtc::GetValueFromJsonObject(json_value, kIceServers,
                                    &json_v_iceservers) &&
        rtc::JsonArrayToValueVector(json_v_iceservers, &iceservers) == true) {
        for (const auto iceserver : iceservers) {
            Json::Value url_value;
            utils::IceServer server;
            if (rtc::GetValueFromJsonObject(iceserver, kIceServersUrls,
                                            &url_value) &&
                url_value.isArray()) {
                rtc::JsonArrayToStringVector(url_value, &server.urls);
                for (auto url : server.urls) {
                    if (validateIceServerUrl(url, error_message) == false) {
                        RTC_LOG(LS_ERROR)
                            << "Error in validate Ice URL: " << error_message;
                        return false;
                    }
                }
            } else {
                std::string url;
                if (rtc::GetStringFromJson(url_value, &url) == true)
                    server.urls.push_back(url);
                else {
                    RTC_LOG(LS_ERROR) << "Failed to get url: "
                                      << rtc::JsonValueToString(url_value);
                    error_message = "failed to get ice server url";
                    return false;
                }
            }
            if (rtc::GetStringFromJsonObject(iceserver, kIceServersUsername,
                                             &server.username) == true) {
                if (rtc::GetStringFromJsonObject(iceserver, kIceServersPassword,
                                                 &server.password) == false) {
                    // password not exist, so try credential
                    rtc::GetStringFromJsonObject(
                        iceserver, kIceServersCredential, &server.password);
                }
            }
            config.servers.push_back(server);
        }
    } else {
        RTC_LOG(INFO) << "ICE SERVERS NONE?: "
                      << rtc::JsonValueToString(json_v_iceservers);
        error_message = "ice server does not exist";
        return false;
    }

    return true;
}

IceTransportsType StrToIceTransportsType(const std::string type) {
    std::map<std::string, IceTransportsType> keyword_map;
    keyword_map["none"] = IceTransportsType::kNone;
    keyword_map["relay"] = IceTransportsType::kRelay;
    keyword_map["nohost"] = IceTransportsType::kNoHost;
    keyword_map["all"] = IceTransportsType::kAll;
    if (keyword_map.find(type) != keyword_map.end())
        return keyword_map[type];
    else {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "TransportsType : " << type
                          << " not found, using default all";
        return IceTransportsType::kAll;
    }
}

BundlePolicy StrToBundlePolicy(const std::string bundle_policy) {
    std::map<std::string, BundlePolicy> keyword_map;
    keyword_map["balanced"] = BundlePolicy::kBundlePolicyBalanced;
    keyword_map["max-bundle"] = BundlePolicy::kBundlePolicyMaxBundle;
    keyword_map["max-compat"] = BundlePolicy::kBundlePolicyMaxCompat;
    if (keyword_map.find(bundle_policy) != keyword_map.end())
        return keyword_map[bundle_policy];
    else {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "BundlePolicy : " << bundle_policy
                          << " not found, using default balanced";
        return BundlePolicy::kBundlePolicyBalanced;
    }
}

RtcpMuxPolicy StrToRtcpMuxPolicy(const std::string mux_policy) {
    std::map<std::string, RtcpMuxPolicy> keyword_map;
    keyword_map["require"] = RtcpMuxPolicy::kRtcpMuxPolicyRequire;
    keyword_map["negotiate"] = RtcpMuxPolicy::kRtcpMuxPolicyNegotiate;
    if (keyword_map.find(mux_policy) != keyword_map.end())
        return keyword_map[mux_policy];
    else {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "MuxPolicy : " << mux_policy
                          << " not found, using default require";
        return RtcpMuxPolicy::kRtcpMuxPolicyRequire;
    }
}

TlsCertPolicy StrToTlsCertPolicy(const std::string cert_poicy) {
    std::map<std::string, TlsCertPolicy> keyword_map;
    keyword_map["secure"] = TlsCertPolicy::kTlsCertPolicySecure;
    keyword_map["insecure"] = TlsCertPolicy::kTlsCertPolicyInsecureNoCheck;
    if (keyword_map.find(cert_poicy) != keyword_map.end())
        return keyword_map[cert_poicy];
    else {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << "CertPolicy : " << cert_poicy
                          << " not found, using default secure";
        return TlsCertPolicy::kTlsCertPolicySecure;
    }
}

std::vector<std::string> StrToIceUrls(const std::string urls) {
    std::vector<std::string> splited_list;
    std::vector<std::string> url_list;

    rtc::split(urls, kConfigDelimiter, &splited_list);
    for (auto token : splited_list) {
        std::string error_message;
        if (validateIceServerUrl(token, error_message) == false) {
            RTC_LOG(LS_ERROR) << "Error in validate Ice URL: " << error_message;
        } else
            url_list.push_back(token);
    }
    return url_list;
}

std::vector<std::string> StrToVector(const std::string configs) {
    std::vector<std::string> splited_list;
    rtc::split(configs, kConfigDelimiter, &splited_list);
    return splited_list;
}

void PrintRTCConfig(const RTCConfiguration& rtc_config) {
    RTC_LOG(INFO) << "RTC Configuration for ICE and ICE servers";
    RTC_LOG(INFO) << "- TransportsType : "
                  << utils::IceTransportsTypeToString(rtc_config.type);
    RTC_LOG(INFO) << "- BundlePolicy : "
                  << utils::BundlePolicyToString(rtc_config.bundle_policy);
    RTC_LOG(INFO) << "- RtcpMuxPolicy : "
                  << utils::RtcpMuxPolicyToString(rtc_config.rtcp_mux_policy);
    RTC_LOG(INFO) << "- ICE server(s) : " << rtc_config.servers.size();

    int server_index = 0;
    for (const auto server : rtc_config.servers) {
        if (!server.urls.empty()) {
            RTC_LOG(INFO) << "- ICE server : #" << server_index;
            for (const std::string& url : server.urls) {
                if (url.empty()) {
                    RTC_LOG(INFO) << "-- url : "
                                  << "Error: url is empty";
                } else {
                    RTC_LOG(INFO) << "-- url : " << url;
                }
            }

            if (!server.username.empty())
                RTC_LOG(INFO) << "-- username : " << server.username;
            if (!server.password.empty())
                RTC_LOG(INFO)
                    << "-- password : " << maskingPassword(server.password);
            if (!server.hostname.empty())
                RTC_LOG(INFO) << "-- hostname : " << server.hostname;
            if (!server.password.empty())
                RTC_LOG(INFO)
                    << "-- Tls Cert Policy : "
                    << utils::TlsCertPolicyToString(server.tls_cert_policy);
            if (server.tls_alpn_protocols.size()) {
                RTC_LOG(INFO) << "-- Tls Alpn Protocols : "
                              << server.tls_alpn_protocols.size();
                for (const std::string& protocols : server.tls_alpn_protocols) {
                    RTC_LOG(INFO) << "--- Alpn Protocols : " << protocols;
                }
            };
            if (server.tls_elliptic_curves.size()) {
                RTC_LOG(INFO) << "-- Tls Elliptic Curves : "
                              << server.tls_elliptic_curves.size();
                for (const std::string& curves : server.tls_elliptic_curves) {
                    RTC_LOG(INFO) << "--- Curves : " << curves;
                }
            }
        } else {
            RTC_LOG(INFO) << "- ICE server : #" << server_index
                          << "Syntax error, urls is empty";
        }
        server_index++;
    }
}

std::string maskingPassword(const std::string password) {
    size_t len = password.length();
    return std::string(len - 2, '*') + password.substr(len - 2, len - 1);
}

bool validateIceServerUrl(const std::string url, std::string& error_message,
                          bool not_allow_numberic_ipaddress) {
    std::vector<std::string> url_splited;
    if (url.empty() ||
        rtc::split(url, kDeliimiterTransport, &url_splited) == 0) {
        error_message = "abnormal ice server url string";
        return false;
    }

    std::vector<std::string> schemeAndHost;
    if (rtc::split(url_splited[0], kDeliimiterSchemeAndPort, &schemeAndHost) <
            1 ||
        schemeAndHost[0].empty() || schemeAndHost[1].empty()) {
        error_message = "no scheme or no host in url";
        return false;
    }

    ServiceSchemeType serviceType = INVALID;
    for (size_t index = 0; index < arraysize(kValidIceServiceTypes); ++index) {
        if (schemeAndHost[0].compare(kValidIceServiceTypes[index]) == 0)
            serviceType = static_cast<ServiceSchemeType>(index);
    }

    if (serviceType == INVALID) {
        error_message = std::string("unknown service scheme type : ")
                            .append(schemeAndHost[0]);
        return false;
    }
    if (not_allow_numberic_ipaddress &&
        (strspn(schemeAndHost[1].c_str(), kSpanNumericIpAddress) ==
         schemeAndHost[1].length())) {
        error_message = std::string("host address is numberic ipaddress: ")
                            .append(schemeAndHost[1]);
        return false;
    }
    return true;
}

};  // namespace utils
