/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <ctype.h>

#include <fstream>
#include <iostream>

#include "rtc_base/logging.h"
#include "rtc_base/stream.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_utils.h"

// clang format off
#include "compat/filestream.h"
#include "compat/optionsfile.h"
// clang format on

const static size_t kConfigLineMinLength = 4;

namespace rtc {

OptionsFile::OptionsFile(const std::string& path) : path_(path) {}

OptionsFile::~OptionsFile() = default;

bool OptionsFile::Load(bool verbose) {
    options_.clear();
    // Open file.
    std::ifstream file(path_);
    if (!file.is_open()) {
        // We do not consider this an error because we expect there to be no
        // file until the user saves a setting.
        return true;
    }

    size_t line_number = 1;
    for (std::string line; std::getline(file, line); line_number++) {
        // ignore the comment
        size_t endpos = line.find_first_of("#");
        if (std::string::npos != endpos) {
            line = line.substr(0, endpos);
        }
        if (line.length() < kConfigLineMinLength) {
            if (verbose) {
                std::cout << __FUNCTION__ << "Ignored #" << line_number
                          << ", comment line or too small  : " << line << "\n";
            }
            continue;  // empty or comment
        };

        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            // Ignore entire line  and keep going
            if (verbose) {
                std::cout << __FUNCTION__ << "Ignored #" << line_number
                          << "line : " << line << "\n";
            }
            continue;
        }

        // trim key/value
        std::string key = rtc::string_trim(std::string(line, 0, equals_pos));
        std::string value = rtc::string_trim(std::string(
            line, equals_pos + 1, line.length() - (equals_pos + 1)));

        if (verbose) {
            std::cout << __FUNCTION__ << "Valid line #" << line_number
                      << "  Key :\"" << key << "\", : Value :\"" << value
                      << "\"\n";
        }
        options_[key] = value;
    }
    return true;
}

bool OptionsFile::Save() {
    // Open file.
    FileStream stream;
    int err;
    if (!stream.Open(path_, "w", &err)) {
        RTC_LOG_F(LS_ERROR) << "Could not open file, err=" << err;
        return false;
    }
    // Write out all the data.
    StreamResult res = SR_SUCCESS;
    size_t written;
    int error;
    for (OptionsMap::const_iterator i = options_.begin(); i != options_.end();
         ++i) {
        res = stream.WriteAll(i->first.c_str(), i->first.length(), &written,
                              &error);
        if (res != SR_SUCCESS) {
            break;
        }
        res = stream.WriteAll("=", 1, &written, &error);
        if (res != SR_SUCCESS) {
            break;
        }
        res = stream.WriteAll(i->second.c_str(), i->second.length(), &written,
                              &error);
        if (res != SR_SUCCESS) {
            break;
        }
        res = stream.WriteAll("\n", 1, &written, &error);
        if (res != SR_SUCCESS) {
            break;
        }
    }
    if (res != SR_SUCCESS) {
        RTC_LOG_F(LS_ERROR) << "Unable to write to file";
        return false;
    } else {
        return true;
    }
}

bool OptionsFile::IsLegalName(const std::string& name) {
    for (size_t pos = 0; pos < name.length(); ++pos) {
        if (name[pos] == '\n' || name[pos] == '\\' || name[pos] == '=') {
            // Illegal character.
            RTC_LOG(LS_WARNING)
                << "Ignoring operation for illegal option " << name;
            return false;
        }
    }
    return true;
}

bool OptionsFile::IsLegalValue(const std::string& value) {
    for (size_t pos = 0; pos < value.length(); ++pos) {
        if (value[pos] == '\n' || value[pos] == '\\') {
            // Illegal character.
            RTC_LOG(LS_WARNING)
                << "Ignoring operation for illegal value " << value;
            return false;
        }
    }
    return true;
}

bool OptionsFile::GetStringValue(const std::string& option,
                                 std::string* out_val) const {
    RTC_LOG(LS_VERBOSE) << "OptionsFile::GetStringValue " << option;
    if (!IsLegalName(option)) {
        return false;
    }
    OptionsMap::const_iterator i = options_.find(option);
    if (i == options_.end()) {
        return false;
    }
    *out_val = i->second;
    RTC_LOG(LS_VERBOSE) << "Value : " << *out_val;
    return true;
}

bool OptionsFile::GetIntValue(const std::string& option, int* out_val) const {
    RTC_LOG(LS_VERBOSE) << "OptionsFile::GetIntValue " << option;
    if (!IsLegalName(option)) {
        return false;
    }
    OptionsMap::const_iterator i = options_.find(option);
    if (i == options_.end()) {
        return false;
    }
    return FromString(i->second, out_val);
}

bool OptionsFile::SetStringValue(const std::string& option,
                                 const std::string& value) {
    RTC_LOG(LS_VERBOSE) << "OptionsFile::SetStringValue " << option << ":"
                        << value;
    if (!IsLegalName(option) || !IsLegalValue(value)) {
        return false;
    }
    options_[option] = value;
    return true;
}

bool OptionsFile::SetIntValue(const std::string& option, int value) {
    RTC_LOG(LS_VERBOSE) << "OptionsFile::SetIntValue " << option << ":"
                        << value;
    if (!IsLegalName(option)) {
        return false;
    }
    options_[option] = ToString(value);
    return true;
}

bool OptionsFile::RemoveValue(const std::string& option) {
    RTC_LOG(LS_VERBOSE) << "OptionsFile::RemoveValue " << option;
    if (!IsLegalName(option)) {
        return false;
    }
    options_.erase(option);
    return true;
}

}  // namespace rtc
