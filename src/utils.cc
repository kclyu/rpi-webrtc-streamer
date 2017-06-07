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
#include "webrtc/base/pathutils.h"
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

    if( MoveLogFiletoNextShiftDir() == false ) {
        LOG(LS_ERROR) << "Failed to rotate log files at path: " << dir_path_;
        return false;
    };

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

bool FileLogger::MoveLogFiles(const std::string prefix, 
            const rtc::Pathname src, const rtc::Pathname dest) {
    rtc::DirectoryIterator it;
    rtc::Pathname src_file, dest_file;

    // check src & dest path is directory
    if( !rtc::Filesystem::IsFolder(dest)) {
        if( rtc::Filesystem::CreateFolder( dest ) == false ) 
            LOG(LS_ERROR) << "Failed to create dest directory: " 
                << dest.pathname();
    }
    if( !rtc::Filesystem::IsFolder(src)) {
        if( rtc::Filesystem::CreateFolder( src ) == false ) {
            LOG(LS_ERROR) << "Failed to create src directory: " 
                << src.pathname();
            // source directory is just created, so nothing to move files
            return true;
        };
    }

    // iterate source directory 
    if (!it.Iterate(src)) {
        return false;
    } 
    do {
        std::string filename = it.Name();
        if( filename.compare(0, prefix.size(), prefix ) == 0 ) {
            src_file.SetPathname(src.folder(), filename);
            dest_file.SetPathname(dest.folder(), filename);
            if( rtc::Filesystem::MoveFile(src_file, dest_file) == false ) 
                LOG(LS_ERROR) << "Failed to move file : " 
                    << src_file.pathname() << ", to: " 
                    << dest_file.pathname();
        };
    } while ( it.Next() );

    return true;
}

bool FileLogger::MoveLogFiletoNextShiftDir() {
    char src_dir_postfix[16], dest_dir_postfix[16];
    rtc::Pathname base_log_path;
    rtc::Pathname src_log_dir, dest_log_dir;

    base_log_path.SetFolder( dir_path_ );
    // checking whether the base log directory is exist
    if( rtc::Filesystem::IsFolder( base_log_path ) == false ) {
        LOG(LS_ERROR) << "Log directory does not exist : " 
                    << base_log_path.pathname() ;
        return false;
    };

    // move log files to next shift directory
    for(int index = 9; index > 0 ; index -- ) {
        rtc::sprintfn(dest_dir_postfix,sizeof(dest_dir_postfix),"0%d", index );
        rtc::sprintfn(src_dir_postfix,sizeof(src_dir_postfix),"0%d", index - 1);
        dest_log_dir.SetFolder(base_log_path.pathname() + dest_dir_postfix );
        src_log_dir.SetFolder(base_log_path.pathname() + src_dir_postfix );

        // remove last rotate directory
        if( index == 9 ) {
            if( rtc::Filesystem::IsFolder(dest_log_dir) == true ) 
                rtc::Filesystem::DeleteFolderContents(dest_log_dir);
        };
        if( MoveLogFiles(LOGGING_FILENAME, src_log_dir, dest_log_dir ) == false ){
            return false;
        }
    };

    // move log file to shit directory
    src_log_dir.SetFolder(dir_path_);
    dest_log_dir.SetFolder(dir_path_+"/00");
    if( MoveLogFiles(LOGGING_FILENAME, src_log_dir, dest_log_dir ) == false ){
        return false;
    }
    return true;
}

};


