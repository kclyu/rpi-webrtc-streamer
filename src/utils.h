/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RPI_STREAMER_UTILS_H_
#define RPI_STREAMER_UTILS_H_

#pragma once

#include <assert.h>
#include <string>

#include "absl/types/optional.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

namespace utils {
void PrintLicenseInfo();
void PrintVersionInfo();

// utility functions
std::string IntToString(int i);
std::string Size_tToString(size_t i);
bool StringToInt(const std::string &str,int *value );
bool ParseVideoResolution(const std::string resolution,int *width, int *height );
rtc::LoggingSeverity String2LogSeverity(const std::string severity);

// Getting folder and parent folder from std::string path
std::string GetFolder(std::string path);
std::string GetParentFolder(std::string path);

// Filesystem access utility helpers
bool IsFolder(const std::string& file);
bool IsFile(const std::string& file);
bool DeleteFile(const std::string& file);
bool MoveFile(const std::string& old_file, const std::string& new_file);
absl::optional<size_t> GetFileSize(const std::string& file);

// Get hardware serial number from /proc/cpu
bool GetHardwareDeviceId(std::string *deviceid);

};

#endif  // RPI_STREAMER_UTILS_H_

