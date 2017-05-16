/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * utils.cc
 *
 * Modified version of webrtc/src/webrtc/examples/peer/client/utils.cc in WebRTC source tree
 * The origianl copyright info below.
 */
/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h> 

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/stringencode.h"

using rtc::ToString;
using rtc::FromString;
using rtc::sprintfn;

std::string int2str(int i) {
    return ToString<int>(i);
}

std::string size_t2str(size_t i) {
    return ToString<size_t>(i);
}

bool string2int(const std::string &str,int *value ) {
    return FromString<int>(str, value);
}

bool parse_resolution(const std::string resolution,int *width, int *height ) {
    const std::string delimiter="x";
    std::string token;
    size_t pos = 0;
    int value;

    if((pos = resolution.find(delimiter)) != std::string::npos)  {
        token = resolution.substr(0, pos);
        // getting width value
        if( string2int( token, &value ) == false ) {
            *width = *height = 0;
            return false;
        }
        *width = value;
    }

    // getting height value
    token = resolution.substr(pos+1, std::string::npos);
    if( string2int( token, &value ) == false ) {
        *width = *height = 0;
        return false;
    }
    *height = value;
    return true;
}

