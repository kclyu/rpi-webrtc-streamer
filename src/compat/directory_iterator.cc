/*
 * Copyright (c) 2018, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * directory_iterator.cc
 *
 * Modified version of rtc_base/fileutils in WebRTC source tree
 * The origianl copyright info below.
 */
/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "compat/directory_iterator.h"

#include <iostream>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_utils.h"

static const char* kDirectoryDelimiter = "/";

namespace utils {

//////////////////////////
// Directory Iterator   //
//////////////////////////

// A DirectoryIterator is created with a given directory. It originally points
// to the first file in the directory, and can be advanecd with Next(). This
// allows you to get information about each file.

// Constructor
DirectoryIterator::DirectoryIterator() : dir_(nullptr), dirent_(nullptr) {}

// Destructor
DirectoryIterator::~DirectoryIterator() {
    if (dir_) closedir(dir_);
}

// Starts traversing a directory.
// dir is the directory to traverse
// returns true if the directory exists and is valid
bool DirectoryIterator::Iterate(const std::string& dir) {
    directory_ = dir;
    if (dir_ != nullptr) closedir(dir_);
    dir_ = ::opendir(directory_.c_str());
    if (dir_ == nullptr) return false;
    dirent_ = readdir(dir_);
    if (dirent_ == nullptr) return false;

    if (::stat(std::string(ValidateDirectoryPath(directory_) + Name()).c_str(),
               &stat_) != 0)
        return false;
    return true;
}

// Advances to the next file
// returns true if there were more files in the directory.
bool DirectoryIterator::Next() {
    dirent_ = ::readdir(dir_);
    if (dirent_ == nullptr) return false;

    return ::stat(
               std::string(ValidateDirectoryPath(directory_) + Name()).c_str(),
               &stat_) == 0;
}

// returns true if the file currently pointed to is a directory
bool DirectoryIterator::IsDirectory() const { return S_ISDIR(stat_.st_mode); }

// returns the name of the file currently pointed to
std::string DirectoryIterator::Name() const {
    RTC_DCHECK(dirent_);
    return dirent_->d_name;
}

// Add a Directory delimiter if it is not at the end of the string
std::string DirectoryIterator::ValidateDirectoryPath(const std::string& dir) {
    std::string::size_type pos = dir.rfind(kDirectoryDelimiter);
    if (pos == (dir.size() - 1)) return dir;
    return dir + kDirectoryDelimiter;
}

}  // namespace utils
