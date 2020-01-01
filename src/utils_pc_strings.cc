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

#include "utils_pc_strings.h"

namespace utils {

const char kDefaultIceTransportsType[] = "all";
const char kDefaultBundlePolicy[] = "balanced";
const char kDefaultRtcpMuxPolicy[] = "require";

std::string SignalingStateToString(const SignalingState state) {
    switch (state) {
        case SignalingState::kStable:
            return "Stable";
        case SignalingState::kHaveLocalOffer:
            return "HaveLocalOffer";
        case SignalingState::kHaveLocalPrAnswer:
            return "HaveLocalPrAnswer";
        case SignalingState::kHaveRemoteOffer:
            return "HaveRemoteOffer";
        case SignalingState::kHaveRemotePrAnswer:
            return "HaveRemotePrAnswer";
        case SignalingState::kClosed:
            return "Closed";
        default:
            return "Unknown";
    };
}

std::string IceGatheringStateToString(const IceGatheringState state) {
    switch (state) {
        case IceGatheringState::kIceGatheringNew:
            return "IceGatheringNoew";
        case IceGatheringState::kIceGatheringGathering:
            return "IceGatherringGathering";
        case IceGatheringState::kIceGatheringComplete:
            return "IceGatheringComplete";
        default:
            return "Unknown";
    };
}

std::string PeerConnectionStateToString(const PeerConnectionState state) {
    switch (state) {
        case PeerConnectionState::kNew:
            return "New";
        case PeerConnectionState::kConnecting:
            return "Connecting";
        case PeerConnectionState::kConnected:
            return "Connected";
        case PeerConnectionState::kDisconnected:
            return "Disconnected";
        case PeerConnectionState::kFailed:
            return "Failed";
        case PeerConnectionState::kClosed:
            return "Closed";
        default:
            return "Unknown";
    };
}

std::string PeerIceConnectionStateToString(const IceConnectionState state) {
    switch (state) {
        case IceConnectionState::kIceConnectionNew:
            return "IceConnectionNew";
        case IceConnectionState::kIceConnectionChecking:
            return "IceConnectionChecking";
        case IceConnectionState::kIceConnectionConnected:
            return "IceConnectionConnected";
        case IceConnectionState::kIceConnectionCompleted:
            return "IceConnectionComplted";
        case IceConnectionState::kIceConnectionFailed:
            return "IceConnectionFailed";
        case IceConnectionState::kIceConnectionDisconnected:
            return "IceConnectionDisconnected";
        case IceConnectionState::kIceConnectionClosed:
            return "IceConnectionClosed";
        case IceConnectionState::kIceConnectionMax:
            return "IceConnectionMax";
        default:
            return "Unknown";
    };
}

// It is not a printer for state, but an Ice related enum printer.
//
std::string IceTransportsTypeToString(const IceTransportsType type,
                                      bool default_value) {
    switch (type) {
        case IceTransportsType::kNone:
            return "none";
        case IceTransportsType::kRelay:
            return "relay";
        case IceTransportsType::kNoHost:
            return "nohost";
        case IceTransportsType::kAll:
            return "all";
        default:
            if (default_value == false)
                return "Unknown";
            else
                return kDefaultIceTransportsType;
    };
}

std::string BundlePolicyToString(const BundlePolicy type, bool default_value) {
    switch (type) {
        case BundlePolicy::kBundlePolicyBalanced:
            return "balanced";
        case BundlePolicy::kBundlePolicyMaxBundle:
            return "max-bundle";
        case BundlePolicy::kBundlePolicyMaxCompat:
            return "max-compat";
        default:
            if (default_value == false)
                return "Unknown";
            else
                return kDefaultBundlePolicy;
    };
}

std::string RtcpMuxPolicyToString(const RtcpMuxPolicy type,
                                  bool default_value) {
    switch (type) {
        case RtcpMuxPolicy::kRtcpMuxPolicyNegotiate:
            return "negotiate";
        case RtcpMuxPolicy::kRtcpMuxPolicyRequire:
            return "require";
        default:
            if (default_value == false)
                return "Unknown";
            else
                return kDefaultRtcpMuxPolicy;
    };
}

std::string TlsCertPolicyToString(const TlsCertPolicy type,
                                  bool default_value) {
    switch (type) {
        case TlsCertPolicy::kTlsCertPolicySecure:
            return "TlsCertPolicySecure";
        case TlsCertPolicy::kTlsCertPolicyInsecureNoCheck:
            return "TlsCertPolicyInsecureNoCheck";
        default:
            return "Unknown";
    };
}

};  // namespace utils
