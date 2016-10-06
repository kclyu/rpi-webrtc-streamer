/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * streamer_def.h
 *
 *
 * Modified version of webrtc/examples/peerconnection/client/defaults.cc
 * in WebRTC source tree
 * The origianl copyright info below.
 */
/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <memory>
#include <string>
#include "webrtc/base/arraysize.h"
#include "webrtc/base/common.h"

#include "streamer_defaults.h"


const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamLabel[] = "stream_label";
const uint16_t kDefaultServerPort = 8888;
const size_t kMaxConnections = (FD_SETSIZE - 2);

// Conductor peerConnection constants
const char kStunIceServer[] = "stun:stun.l.google.com:19302";

//
// TURN Server
//
// Not used
const char kTurnIceServer[] = "turn:turn.hostname:3478?transport=tcp";
const char kTurnUsername[] = "username";
const char kTurnPassword[] = "password";

std::string GetEnvVarOrDefault(const char* env_var_name, const char* default_value) {
    std::string value;
    const char* env_var = getenv(env_var_name);
    if (env_var)
        value = env_var;

    if (value.empty())
        value = default_value;

    return value;
}

bool GetEnvVarOrDefaultBool(const char* env_var_name, const char* default_value) {
    std::string value;
    const char* env_var = getenv(env_var_name);
    if (env_var)
        value = env_var;

    if (value.empty())
        value = default_value;

    if( value.compare("TRUE") == 0 ) return true;
    return false;
}

std::string GetPeerConnectionString() {
    return GetEnvVarOrDefault("WEBRTC_CONNECT", "stun:stun.l.google.com:19302");
}

bool GetDTLSEnableBool() {
    return GetEnvVarOrDefaultBool("WEBRTC_DTLS", "TRUE");
}

std::string GetDefaultTurnServerString() {
    return GetEnvVarOrDefault("WEBRTC_TURN", kTurnIceServer);
}

std::string GetDefaultTurnServerUserString() {
    return GetEnvVarOrDefault("WEBRTC_TURN_USER",kTurnUsername );
}

std::string GetDefaultTurnServerPassString() {
    return GetEnvVarOrDefault("WEBRTC_TURN_PASS", kTurnPassword );
}

std::string GetDefaultServerName() {
    return GetEnvVarOrDefault("WEBRTC_SERVER", "10.0.0.10");
}

std::string GetPeerName() {
    char computer_name[256];
    std::string ret(GetEnvVarOrDefault("USERNAME", "user"));
    ret += '@';
    if (gethostname(computer_name, arraysize(computer_name)) == 0) {
        ret += computer_name;
    } else {
        ret += "host";
    }
    return ret;
}



