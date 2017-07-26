/* 
 * Copyright (c) 2017, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * main.cc
 *
 * Modified version of webrtc/src/webrtc/examples/peer/client/main.cc in WebRTC source tree
 * The origianl copyright info below.
 */
/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "webrtc/rtc_base/network.h"
#include "webrtc/rtc_base/nethelpers.h"
#include "webrtc/rtc_base/networkmonitor.h"
#include "webrtc/rtc_base/physicalsocketserver.h"
#include "webrtc/rtc_base/signalthread.h"
#include "webrtc/rtc_base/sigslot.h"
#include "webrtc/rtc_base/ssladapter.h"
#if defined(WEBRTC_POSIX)
#include <sys/types.h>
#include <net/if.h>
#include "webrtc/rtc_base/ifaddrs_converter.h"
#endif  // defined(WEBRTC_POSIX)


#include "webrtc/rtc_base/flags.h"

#include "websocket_server.h"
#include "app_channel.h"

#include "streamer_observer.h"
#include "direct_socket.h"
#include "streamer_config.h"
#include "media_config.h"
#include "streamer.h"
#include "utils.h"
#include "file_logger.h"


// 
//
class StreamingSocketServer : public rtc::PhysicalSocketServer {
public:
    explicit StreamingSocketServer()
        : websocket_(nullptr)  {}
    virtual ~StreamingSocketServer() {}

    // TODO: Quit feature implementation using message queue is required.
    void SetMessageQueue(rtc::MessageQueue* queue) override {
        message_queue_ = queue;
    }

    void set_websocket_server(LibWebSocketServer* websocket) { websocket_ = websocket; }

    virtual bool Wait(int cms, bool process_io) {
        if( websocket_ )
            websocket_->RunLoop(0); // Run Websocket loop once per call
        return rtc::PhysicalSocketServer::Wait(0/*cms == -1 ? 1 : cms*/,
                process_io);
    }

protected:
    rtc::MessageQueue* message_queue_;
    std::unique_ptr<rtc::AsyncSocket> listener_;
    LibWebSocketServer *websocket_;
};

//
//  flags definition for streamer
// 
DEFINE_bool(help, false, "Prints this message");
DEFINE_bool(verbose, false, "Print logging message on stdout");
DEFINE_string(conf, "etc/webrtc_streamer.conf",
           "the main configuration file for webrtc-streamer");
DEFINE_string(severity, "INFO",
           "logging message severity level(VERBOSE,INFO,WARNING,ERROR)");
DEFINE_string(log, "log", "directory for logging message");
DEFINE_bool(licenses, false, "print the LICENSE information");
DEFINE_bool(version, false, "print the Version information");

//
// Main
//
int main(int argc, char** argv) {
    std::string media_config;
    std::string baselog_dir;
    rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);

    if( FLAG_help ) {
        rtc::FlagList::Print(NULL, false);
        return 0;
    }
    
    if( FLAG_licenses ) {
        utils::PrintLicenseInfo();
        return 0;
    };
    
    if( FLAG_version ) {
        utils::PrintVersionInfo();
        return 0;
    };

    // Load the streamer configuration from file
    StreamerConfig streamer_config(FLAG_conf);
    if( streamer_config.LoadConfig() == false ) {
        std::cerr  << "Failed to load config options:" 
                << streamer_config.GetConfigFilename() << "\n";
        return -1;
    };

    // Getting the log directory path 
    // The priorities are as follows.
    //      1. Use command line flag when path of command line flag exists
    //      2. If not, use the INSTALL_DIR + log path
    baselog_dir = FLAG_log;
    if( streamer_config.GetLogPath(baselog_dir) == false ) {
        std::cerr << "Failed to get log directory : " 
            << baselog_dir << "\n";
        return -1;
    };

    rtc::LoggingSeverity severity = utils::String2LogSeverity(FLAG_severity);
    utils::FileLogger file_logger(baselog_dir, severity, 
            streamer_config.GetDisableLogBuffering());
    if( FLAG_verbose )  {
        // changing severity to INFO level
        severity = utils::String2LogSeverity("INFO");
        rtc::LogMessage::LogToDebug(severity);
    }
    else {
        // file logging will be enabled only when verbose flag is disabled.
        if( !file_logger.Init() ) {
            std::cerr << "Failed to init file message logger\n" ;
            return -1;
        }
    }

    if( streamer_config.GetMediaConfig(media_config) == true ) {
        if( media_config::config_load(media_config) == false ) {
            LOG(LS_WARNING) << "Failed to load config options:" << media_config;
        }
        LOG(INFO) << "Using Media Config file: " << media_config;
    };

    std::unique_ptr<DirectSocketServer> direct_socket_server;
    std::unique_ptr<AppChannel> app_channel;

    StreamingSocketServer socket_server;
    rtc::AutoSocketServerThread thread(&socket_server);

    rtc::InitializeSSL();

    // DirectSocket 
    if(streamer_config.GetDirectSocketEnable() == true) {
        int direct_socket_port_num;

        if( !streamer_config.GetDirectSocketPort(direct_socket_port_num) ) {
            LOG(LS_ERROR) << "Error in getting direct socket port number: " 
                << direct_socket_port_num;
            rtc::CleanupSSL();
            return -1;
        }
        LOG(INFO) << "Direct socket port num: " << direct_socket_port_num;
        rtc::SocketAddress addr( "0.0.0.0", direct_socket_port_num );

        direct_socket_server.reset(new DirectSocketServer());
        if (direct_socket_server->Listen(addr) == false) {
            rtc::CleanupSSL();
            return -1;
        }
    }

    // WebSocket
    if( streamer_config.GetWebSocketEnable() == true ) {
        app_channel.reset(new AppChannel());
        app_channel->AppInitialize(streamer_config);

        socket_server.set_websocket_server(app_channel.get());
    };

    rtc::scoped_refptr<Streamer> streamer(
        new rtc::RefCountedObject<Streamer>(StreamerProxy::GetInstance(), 
        &streamer_config));

    // Running Loop
    thread.Run();

    rtc::CleanupSSL();
    return 0;
}

