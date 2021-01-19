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

#ifndef STREAMER_UTILS_PC_CONFIG_H_
#define STREAMER_UTILS_PC_CONFIG_H_

#pragma once

#include <assert.h>

#include <string>

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"

namespace utils {

// typedef aliases for webrtc::PeerConnectionInterface structures
typedef webrtc::PeerConnectionInterface::SignalingState SignalingState;
typedef webrtc::PeerConnectionInterface::IceGatheringState IceGatheringState;
typedef webrtc::PeerConnectionInterface::PeerConnectionState
    PeerConnectionState;
typedef webrtc::PeerConnectionInterface::IceConnectionState IceConnectionState;

typedef webrtc::PeerConnectionInterface::RTCConfiguration RTCConfiguration;
typedef webrtc::PeerConnectionInterface::IceTransportsType IceTransportsType;
typedef webrtc::PeerConnectionInterface::BundlePolicy BundlePolicy;
typedef webrtc::PeerConnectionInterface::RtcpMuxPolicy RtcpMuxPolicy;
typedef webrtc::PeerConnectionInterface::TlsCertPolicy TlsCertPolicy;
typedef webrtc::PeerConnectionInterface::IceServer IceServer;
typedef webrtc::PeerConnectionInterface::IceServers IceServers;

extern const char kDefaultIceTransportsType[];
extern const char kDefaultBundlePolicy[];
extern const char kDefaultRtcpMuxPolicy[];

bool RTCConfigFromJson(utils::RTCConfiguration& config,
                       const std::string& json_string,
                       std::string& error_message);

IceTransportsType StrToIceTransportsType(const std::string type);
BundlePolicy StrToBundlePolicy(const std::string bundle_policy);
RtcpMuxPolicy StrToRtcpMuxPolicy(const std::string mux_policy);

// for ice servers
std::vector<std::string> StrToIceUrls(const std::string urls);
TlsCertPolicy StrToTlsCertPolicy(const std::string policy);
std::vector<std::string> StrToVector(const std::string configs);

std::string maskingPassword(const std::string password);
void PrintRTCConfig(const RTCConfiguration& rtc_config);
bool validateIceServerUrl(const std::string url, std::string& error_message,
                          bool not_allow_numberic_ipaddress = true);

};  // namespace utils

#endif  // STREAMER_UTILS_PC_CONFIG_H_
