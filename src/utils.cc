/*
 *  Copyright (c) 2017, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * utils.cc
 *
 * Modified version of src/webrtc/examples/peer/client/utils.cc in WebRTC source tree
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
#include <fstream>

#include <sys/stat.h>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#include "rtc_base/stringutils.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/filerotatingstream.h"
#include "rtc_base/logsinks.h"

#include "utils.h"

namespace utils {

static const char FOLDER_DELIMS = '/';

#ifdef LOCAL_DEVICEID
static const char *kCPUInfoPath = "./cpuinfo";  // testing purpose
#else
static const char *kCPUInfoPath = "/proc/cpuinfo";
#endif // LOCAL_DEVICEID
static const char *kCPUInfoSeperator = ":";
static const char *kCPUInfoDeviceToken = "Serial";

// Print verion information for command line option
void PrintVersionInfo() {
    std::cout << "RWS Version: " << __RWS_VERSION__ << std::endl;
    std::cout << "RWS (rpi-webrtc-streamer) is a WebRTC H.264 streamer designed "
        << "to run on Raspberry PI \nand Raspberry PI camera board hardware."
        << std::endl;
    std::cout <<
        "(Using WebRTC native package: " << __WEBRTC_VERSION__ << ")" << std::endl;
}

// Print licenses information for command line option
void PrintLicenseInfo() {
    char command_buffer[1024];
    int ret;
    snprintf( command_buffer, sizeof(command_buffer),
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
    return rtc::ToString(i);
}

std::string Size_tToString(size_t i) {
    return rtc::ToString(i);
}

bool StringToInt(const std::string &str,int *value ) {
    return rtc::FromString<int>(str, value);
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

bool IsFolder(const std::string& file) {
    struct stat st;
    int res = ::stat(file.c_str(), &st);
    return res == 0 && S_ISDIR(st.st_mode);
}

std::string GetFolder(std::string path) {
    return path.substr(0, path.find_last_of(FOLDER_DELIMS));
}

std::string GetParentFolder(std::string path) {
    std::string::size_type pos = std::string::npos;
    std::string folder = GetFolder(path);
    if (folder.size() >= 2) {
        pos = folder.find_last_of(FOLDER_DELIMS, folder.length() - 2);
    }
    if (pos != std::string::npos) {
        return folder.substr(0, pos + 1);
    }
    return "";  // return empty string
}


// Get device id from raspberrypi's cpuinfo
bool GetHardwareDeviceId(std::string *deviceid) {
    std::ifstream file(kCPUInfoPath);
    if(file.is_open()) {
        for (std::string line; std::getline(file, line); ) {
            size_t equals_pos = line.find(kCPUInfoSeperator);
            if (equals_pos == std::string::npos) {
                continue;
            };
            std::string key = rtc::string_trim(std::string(line, 0, equals_pos));
            std::string value = rtc::string_trim(
                    std::string(line, equals_pos + 1, line.length() - (equals_pos + 1)));
            if( key.compare(kCPUInfoDeviceToken) == 0 ) {
                *deviceid = value;
                return true;
            };
        }
    }
    else {
        RTC_LOG(LS_ERROR) << "Could not open CPU info file";
        return false;
    }
    RTC_LOG(LS_ERROR) << "Error in gettting the Hardware DeviceID from cpuinfo";
    return false;
}

};  // namespace utils


