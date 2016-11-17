/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
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

#include <vector>

#include "webrtc/base/nethelpers.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/ssladapter.h"

#include "stream_data_sockets.h"
#include "streamer_defaults.h"
#include "streamer_flagdefs.h"
#include "streamer.h"


class StreamingSocketServer : public rtc::PhysicalSocketServer,
    public sigslot::has_slots<> {
public:
    StreamingSocketServer(rtc::Thread* thread)
        : thread_(thread)  {}
    virtual ~StreamingSocketServer() {}

protected:
    std::unique_ptr<rtc::AsyncSocket> listener_;
    rtc::Thread* thread_;
};



int main(int argc, char** argv) {

    rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
    if (FLAG_help) {
        rtc::FlagList::Print(NULL, false);
        return 0;
    }

    // Abort if the user specifies a port that is outside the allowed
    // range [1, 65535].
    if ((FLAG_port < 1) || (FLAG_port > 65535)) {
        printf("Error: %i is not a valid port.\n", FLAG_port);
        return -1;
    }

    rtc::AutoThread auto_thread;
    rtc::Thread* thread = rtc::Thread::Current();
    StreamingSocketServer socket_server(thread);
    rtc::SocketAddress addr( FLAG_bind, FLAG_port );
    thread->set_socketserver(&socket_server);

    rtc::InitializeSSL();

    // Must be constructed after we set the socketserver.
    StreamSocketListen stream_listen;

    rtc::scoped_refptr<Streamer> streamer(
        new rtc::RefCountedObject<Streamer>(StreamSession::GetInstance()));

    stream_listen.Listen(addr);
    // Running Loop
    thread->Run();

    // Terminating clean-up
    thread->set_socketserver(NULL);
    rtc::CleanupSSL();

    return 0;
}


