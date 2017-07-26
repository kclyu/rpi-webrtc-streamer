/*
 *  Copyright (c) 2017, rpi-webrtc-streamer  Lyu,KeunChang
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

#include <stdio.h>
#include <stdlib.h> 
#include <memory>
#include <string>
#include <iostream>

#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"

#include "webrtc/rtc_base/stringutils.h"
#include "webrtc/rtc_base/stringencode.h"
#include "webrtc/rtc_base/filerotatingstream.h"
#include "webrtc/rtc_base/pathutils.h"
#include "webrtc/rtc_base/logsinks.h"

#include "utils.h"

using rtc::ToString;
using rtc::FromString;
using rtc::sprintfn;

namespace utils {

// Print verion information for command line option
void PrintVersionInfo() {
    std::cout << "RWS " << __RWS_VERSION__ << std::endl;
    std::cout << "RWS (rpi-webrtc-streamer) is a WebRTC H.264 streamer designed "
        << " to run on Raspberry PI \nand Raspberry PI camera board hardware." 
        << std::endl;
    std::cout << 
        "(Using WebRTC native package: " << __WEBRTC_VERSION__ << ")" << std::endl;
}

// Print licenses information for command line option
void PrintLicenseInfo() {
    char command_buffer[1024];
    int ret;
    sprintfn( command_buffer, sizeof(command_buffer), 
        "find %s/LICENSE -type f -printf \"********************\\n\
********************   %%f\\n********************\\n\\n\" \
-exec cat {} \\; | pager",
        INSTALL_DIR);
    ret = system(command_buffer);
    if( ret !=0 ) {
        std::cout << "Failed to dispaly LICENSE Information file" << std::endl;
    }
}

std::string IntToString(int i) {
    return ToString<int>(i);
}

std::string Size_tToString(size_t i) {
    return ToString<size_t>(i);
}

bool StringToInt(const std::string &str,int *value ) {
    return FromString<int>(str, value);
}

rtc::LoggingSeverity String2LogSeverity(const std::string severity) {
    if(severity.compare("VERBOSE") == 0 ) {
        return rtc::LoggingSeverity::LS_VERBOSE;
    }
    else if(severity.compare("INFO") == 0 ) {
        return rtc::LoggingSeverity::LS_INFO;
    }
    else if(severity.compare("WARNING") == 0 ) {
        return rtc::LoggingSeverity::LS_WARNING;
    }
    else if(severity.compare("ERROR") == 0 ) {
        return rtc::LoggingSeverity::LS_ERROR;
    }
    return rtc::LoggingSeverity::LS_NONE;
}

bool ParseVideoResolution(const std::string resolution,int *width, int *height ) {
    const std::string delimiter="x";
    std::string token;
    size_t pos = 0;
    int value;

    if((pos = resolution.find(delimiter)) != std::string::npos)  {
        token = resolution.substr(0, pos);
        // getting width value
        if( StringToInt( token, &value ) == false ) {
            *width = *height = 0;
            return false;
        }
        *width = value;
    }

    // getting height value
    token = resolution.substr(pos+1, std::string::npos);
    if( StringToInt( token, &value ) == false ) {
        *width = *height = 0;
        return false;
    }
    *height = value;
    return true;
}

};


