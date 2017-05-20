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

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

#include "webrtc/base/stringutils.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/filerotatingstream.h"
#include "webrtc/base/logsinks.h"

#include "utils.h"

#define  LOGGING_FILENAME       "rws_log"
#define  MAX_LOG_FILE_SIZE      10*1024*1024    // 10M bytes;
#define  MAX_LOG_FILE_NUM       10

using rtc::ToString;
using rtc::FromString;
using rtc::sprintfn;

namespace utils {

std::string IntToString(int i) {
    return ToString<int>(i);
}

std::string Size_tToString(size_t i) {
    return ToString<size_t>(i);
}

bool StringToInt(const std::string &str,int *value ) {
    return FromString<int>(str, value);
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


//////////////////////////////////////////////////////////////////////////////////////////
//
// File Logger Class
//
//////////////////////////////////////////////////////////////////////////////////////////
FileLogger::FileLogger (const std::string path,
        const rtc::LoggingSeverity severity) :  
    inited_(false), dir_path_(path), severity_(severity),
    log_max_file_size_(MAX_LOG_FILE_SIZE) {
}

FileLogger::~FileLogger () {
}

bool FileLogger::Init() {
    if( inited_ == true ) return true;  // no more initialization
    logSink_.reset( new rtc::FileRotatingLogSink(dir_path_,
                LOGGING_FILENAME, log_max_file_size_, MAX_LOG_FILE_NUM ));

    if(!logSink_->Init()) {
        LOG(LS_ERROR) << "Failed to open log files at path: " << dir_path_;
        logSink_.reset();
        return false;
    }

    rtc::LogMessage::LogThreads(true);
    rtc::LogMessage::LogTimestamps(true);
    rtc::LogMessage::AddLogToStream(logSink_.get(), severity_);
    inited_ = true;
    return true;
}


};


