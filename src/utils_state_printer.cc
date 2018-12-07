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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "utils_state_printer.h"

namespace utils {

std::string PrintSignalingState(
        const webrtc::PeerConnectionInterface::SignalingState state) {
switch(state) {
    case webrtc::PeerConnectionInterface::SignalingState::kStable:
        return "Stable";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveLocalOffer:
        return "HaveLocalOffer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveLocalPrAnswer:
        return "HaveLocalPrAnswer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer:
        return "HaveRemoteOffer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveRemotePrAnswer:
        return "HaveRemotePrAnswer";
    case webrtc::PeerConnectionInterface::SignalingState::kClosed:
        return "Closed";
    default:
        return "Unknown";
};
}

std::string PrintIceGatheringState(
        const webrtc::PeerConnectionInterface::IceGatheringState state) {
switch(state) {
    case webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringNew:
        return "IceGatheringNoew";
    case webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringGathering:
        return "IceGatherringGathering";
    case webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringComplete:
        return "IceGatheringComplete";
    default:
        return "Unknown";
};
}

std::string PrintPeerConnectionState(
        const webrtc::PeerConnectionInterface::PeerConnectionState state) {
switch(state) {
    case webrtc::PeerConnectionInterface::PeerConnectionState::kNew:
        return "New";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kConnecting:
        return "Connecting";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kConnected:
        return "Connected";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected:
        return "Disconnected";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kFailed:
        return "Failed";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kClosed:
        return "Closed";
    default:
        return "Unknown";
};
}

std::string PrintPeerIceConnectionState(
        const webrtc::PeerConnectionInterface::IceConnectionState state) {
switch(state) {
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew:
        return "IceConnectionNew";
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionChecking:
        return "IceConnectionChecking";
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionConnected:
        return "IceConnectionConnected";
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted:
        return "IceConnectionComplted";
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionFailed:
        return "IceConnectionFailed";
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected:
        return "IceConnectionDisconnected";
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionClosed:
        return "IceConnectionClosed";
    case webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionMax:
        return "IceConnectionMax";
    default:
        return "Unknown";
};
}


};  // utils namespace


