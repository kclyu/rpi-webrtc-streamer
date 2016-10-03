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

#include "stream_data_sockets.h"
#include "utils.h"

#include <stdio.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/stringencode.h"

using rtc::ToString;
using rtc::sprintfn;

std::string int2str(int i) {
  return ToString<int>(i);
}

std::string size_t2str(size_t i) {
  return ToString<size_t>(i);
}

const char *str_stream_connection_method(const long request_method) {
	switch( request_method ) {
		case METHOD_INVALID:
			return "METHOD_INVALID";
		case METHOD_GET:
			return  "METHOD_GET";
		case METHOD_POST:
			return  "METHOD_POST";
		case METHOD_OPTIONS:
			return "METHOD_OPTIONS";
	}
	return "METHOD_UNKNOWN";
}
