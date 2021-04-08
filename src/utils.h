/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef STREAMER_UTILS_H_
#define STREAMER_UTILS_H_

#include <assert.h>
#include <unistd.h>

#include <string>

#include "absl/types/optional.h"
#include "rtc_base/logging.h"

namespace utils {

void PrintLicenseInfo();
std::string GetVersionInfo();
const char* GetProgramDescription();

// utility functions
bool ParseVideoResolution(const std::string resolution, int* width,
                          int* height);
rtc::LoggingSeverity String2LogSeverity(const std::string severity);

const std::string GetDateTimeString(void);

// Getting folder and parent folder from std::string path
std::string GetFolder(std::string path);
std::string GetParentFolder(std::string path);

// Filesystem access utility helpers
bool IsFolder(const std::string& file);
bool IsFile(const std::string& file);
bool DeleteFile(const std::string& file);
bool MoveFile(const std::string& old_file, const std::string& new_file);
bool SymLink(const std::string& origin, const std::string& symbolic_link);
bool GetFolderWithTailingDelimiter(const std::string& path,
                                   std::string& path_with_delimiter);
absl::optional<size_t> GetFileSize(const std::string& file);
absl::optional<time_t> GetFileChangedTime(const std::string& file);

// Get hardware serial number from /proc/cpu
bool GetHardwareDeviceId(std::string* deviceid);

};  // namespace utils

#endif  // STREAMER_UTILS_H_
