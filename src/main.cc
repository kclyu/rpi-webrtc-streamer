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

#include "webrtc/base/network.h"
#include "webrtc/base/nethelpers.h"
#include "webrtc/base/networkmonitor.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/ssladapter.h"
#if defined(WEBRTC_POSIX)
#include <sys/types.h>
#include <net/if.h>
#include "webrtc/base/ifaddrs_converter.h"
#endif  // defined(WEBRTC_POSIX)


#include "webrtc/base/flags.h"

#include "websocket_server.h"
#include "app_channel.h"

#include "streamer_observer.h"
#include "direct_socket.h"
#include "streamer_config.h"
#include "streamer.h"

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

    void set_websocket(LibWebSocketServer* websocket) { websocket_ = websocket; }

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
DEFINE_string(conf, "etc/webrtc-streamer.conf",
           "the main configuration file for webrtc-streamer.");

//
// Main
//
int main(int argc, char** argv) {
    std::string app_channel_config;
    int  websocket_port_num;
    rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
    if (FLAG_help) {
        rtc::FlagList::Print(NULL, false);
        return 0;
    }

    std::unique_ptr<DirectSocketServer> direct_socket_server;
    std::unique_ptr<AppChannel> app_channel;

    StreamingSocketServer socket_server;
    rtc::AutoSocketServerThread thread(&socket_server);

    rtc::InitializeSSL();

    // Load the streamer configuration from file
    StreamerConfig streamer_config(FLAG_conf);

    // DirectSocket 
    if(streamer_config.GetDirectSocketEnable() == true) {
        int direct_socket_port_num;

        if( !streamer_config.GetDirectSocketPort(direct_socket_port_num) ) {
            LOG(LS_ERROR) << "Error in getting direct socket port number: " 
                << direct_socket_port_num;
            rtc::CleanupSSL();
            return -1;
        }
        LOG(INFO) << "Direct socket using port num: " << direct_socket_port_num;
        rtc::SocketAddress addr( "0.0.0.0", direct_socket_port_num );

        direct_socket_server.reset(new DirectSocketServer());
        if (direct_socket_server->Listen(addr) == false) {
            rtc::CleanupSSL();
            return -1;
        }
    }

    // WebSocket
    if( streamer_config.GetWebSocketEnable() == true ) {
        streamer_config.GetAppChannelConfig(app_channel_config);
        streamer_config.GetWebSocketPort(websocket_port_num);
        LOG(INFO) << "WebSocket Using Port : " << websocket_port_num 
            << ", Configuration file : " << app_channel_config;
        app_channel.reset(new AppChannel(websocket_port_num, app_channel_config));
        app_channel->AppInitialize();

        socket_server.set_websocket(app_channel.get());
    };

    rtc::scoped_refptr<Streamer> streamer(
        new rtc::RefCountedObject<Streamer>(StreamerProxy::GetInstance(), 
        &streamer_config));

    // Running Loop
    thread.Run();

    rtc::CleanupSSL();
    return 0;
}

