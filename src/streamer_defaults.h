
#ifndef RPI_STREAMER_DEFAULTS_
#define RPI_STREAMER_DEFAULTS_
#pragma once

#include "webrtc/base/flags.h"

extern const char kAudioLabel[];
extern const char kVideoLabel[];
extern const char kStreamLabel[];
extern const uint16_t kDefaultServerPort;  // From defaults.[h|cc]

// Define flags for the peerconnect_client testing tool, in a separate
// header file so that they can be shared across the different main.cc's
// for each platform.

std::string GetEnvVarOrDefault(const char* env_var_name,
                               const char* default_value);
std::string GetPeerConnectionString();
std::string GetDefaultServerName();
std::string GetPeerName();
bool GetDTLSEnableBool();

std::string GetDefaultTurnServerString();
std::string GetDefaultTurnServerUserString();
std::string GetDefaultTurnServerPassString();


#endif  // RPI_STREAMER_DEFAULTS_
