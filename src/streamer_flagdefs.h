/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * streamer_flagdefs.h
 *
 *
 * Modified version of webrtc/src/webrtc/examples/
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

#ifndef RPI_STREAMER_FLAGSDEFS_H_
#define RPI_STREAMER_FLAGSDEFS_H_
#pragma once

#include "webrtc/base/flags.h"

extern const uint16_t kDefaultServerPort;  // From defaults.[h|cc]

// Define flags for the peerconnect_client testing tool, in a separate
// header file so that they can be shared across the different main.cc's
// for each platform.

DEFINE_bool(help, false, "Prints this message");
DEFINE_string(bind, "127.0.0.1", "The default binding IP address.");
DEFINE_int(port, kDefaultServerPort,
           "The port on which the server is listening.");

#endif  // RPI_STREAMER_FLAGSDEFS_H_
