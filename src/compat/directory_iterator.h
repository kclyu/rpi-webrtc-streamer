/*
 * Copyright (c) 2018, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * directory_iterator.h
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

#ifndef DIRECTORY_ITERATOR_H_
#define DIRECTORY_ITERATOR_H_

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "compat/platform_file.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"

namespace utils {

//////////////////////////
// Directory Iterator   //
//////////////////////////

// A DirectoryIterator is created with a given directory. It originally points
// to the first file in the directory, and can be advanecd with Next(). This
// allows you to get information about each file.

class DirectoryIterator {
   public:
    // Constructor
    DirectoryIterator();
    // Destructor
    virtual ~DirectoryIterator();

    // Starts traversing a directory
    // dir is the directory to traverse
    // returns true if the directory exists and is valid
    // The iterator will point to the first entry in the directory
    virtual bool Iterate(const std::string& path);

    // Advances to the next file
    // returns true if there were more files in the directory.
    virtual bool Next();

    // returns true if the file currently pointed to is a directory
    virtual bool IsDirectory() const;

    // returns the name of the file currently pointed to
    virtual std::string Name() const;

   private:
    std::string ValidateDirectoryPath(const std::string& dir);
    std::string directory_;
    DIR* dir_;
    struct dirent* dirent_;
    struct stat stat_;
};

}  // namespace utils

#endif  // DIRECTORY_ITERATOR_H_
