/*
Copyright (c) 2017, rpi-webrtc-streamer Lyu,KeunChang

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CONFIG_DEFINES_H_
#define CONFIG_DEFINES_H_
#include <string>

#include "compat/optionsfile.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"

///////////////////////////////////////////////////////////////////////////////////////////
//
// macro to define configuration
//
///////////////////////////////////////////////////////////////////////////////////////////
// For config source file definition
#define CONFIG_DEFINE(name, config_var, config_type, default_value) \
    static const char kConfig##name[] = #config_var;                \
    static const config_type kDefault##name = default_value;        \
    static bool config_loaded__##name = false;                      \
    config_type config_var = default_value;                         \
    bool IsLoaded__##name(void) { return config_loaded__##name; }

// For config include file definition
#define CONFIG_DEFINE_H(name, config_var, config_type) \
    extern config_type config_var;                     \
    bool IsLoaded__##name(void);

#define CONFIG_LOAD_BOOL(name, config_var)                                \
    {                                                                     \
        std::string flag_value;                                           \
        if (config_.GetStringValue(kConfig##name, &flag_value) == true) { \
            if (flag_value.compare("true") == 0)                          \
                config_var = true;                                        \
            else if (flag_value.compare("false") == 0)                    \
                config_var = false;                                       \
            else {                                                        \
                RTC_LOG(INFO) << "Default Config \"" << kConfig##name     \
                              << "\" value is not valid" << flag_value;   \
            }                                                             \
        };                                                                \
    };

#define CONFIG_LOAD_BOOL_WITH_DEFAULT(name, config_var, default_value)    \
    {                                                                     \
        std::string flag_value;                                           \
        if (config_.GetStringValue(kConfig##name, &flag_value) == true) { \
            if (flag_value.compare("true") == 0)                          \
                config_var = true;                                        \
            else if (flag_value.compare("false") == 0)                    \
                config_var = false;                                       \
            else {                                                        \
                RTC_LOG(INFO) << "Default Config \"" << kConfig##name     \
                              << "\" value is not valid" << flag_value;   \
                config_var = default_value;                               \
            };                                                            \
        };                                                                \
    };

#define CONFIG_LOAD_INT_WITH_DEFAULT(name, config_var, validate_function, \
                                     default_value)                       \
    {                                                                     \
        if (config_.GetIntValue(kConfig##name, &config_var) == true) {    \
            validate_function(config_var, default_value);                 \
        } else                                                            \
            config_var = default_value;                                   \
    };

////////////////////////////////////////////////////////////////////////////////
//
// macro to load configuration
//
////////////////////////////////////////////////////////////////////////////////

// macro for dummping config name and config_var
// need to insert the "#define CONFIG_LOAD_DUMP" before including this header
// file
#ifdef CONFIG_LOAD_DUMP
#define DUMP_KEY_AND_VALUE(name, conf, loaded_value)                         \
    RTC_LOG(INFO) << "Key \"" << kConfig##name << "\", In conf : \"" << conf \
                  << "\", Value (" << config_loaded__##name << ") : \""      \
                  << loaded_value << "\"";

#else
#define DUMP_KEY_AND_VALUE(dkey, conf, dvalue)
#endif

//
// Macros for loading config
//
#define DEFINE_CONFIG_LOAD_BOOL(name, config_var)                         \
    {                                                                     \
        std::string flag_value;                                           \
        if (config_.GetStringValue(kConfig##name, &flag_value) == true) { \
            config_loaded__##name = true;                                 \
            if (flag_value.compare("true") == 0)                          \
                config_var = true;                                        \
            else if (flag_value.compare("false") == 0)                    \
                config_var = false;                                       \
            else {                                                        \
                RTC_LOG(INFO) << "Default Config \"" << kConfig##name     \
                              << "\" value is not valid" << flag_value;   \
                config_var = kDefault##name;                              \
                config_loaded__##name = false;                            \
            };                                                            \
        };                                                                \
        DUMP_KEY_AND_VALUE(name, flag_value, config_var);                 \
    };

#define DEFINE_CONFIG_LOAD_INT(name, config_var)                        \
    {                                                                   \
        if (config_.GetIntValue(kConfig##name, &config_var) == false) { \
            config_var = kDefault##name;                                \
        } else {                                                        \
            config_loaded__##name = true;                               \
        }                                                               \
        DUMP_KEY_AND_VALUE(name, "n/a", config_var);                    \
    };

#define DEFINE_CONFIG_LOAD_FLOAT(name, config_var)                        \
    {                                                                     \
        std::string flag_value;                                           \
        if (config_.GetStringValue(kConfig##name, &flag_value) == true) { \
            config_loaded__##name = true;                                 \
            if (rtc::FromString(flag_value, &config_var) == false) {      \
                RTC_LOG(INFO) << "Invalid config \"" << kConfig##name     \
                              << "\", value : " << flag_value;            \
                RTC_LOG(INFO) << "Default Config \"" << kConfig##name     \
                              << "\" value is not valid" << flag_value;   \
                config_var = kDefault##name;                              \
                config_loaded__##name = false;                            \
            }                                                             \
        }                                                                 \
        DUMP_KEY_AND_VALUE(name, flag_value, config_var);                 \
    };

#define DEFINE_CONFIG_LOAD_STR(name, config_var)                           \
    {                                                                      \
        if (config_.GetStringValue(kConfig##name, &config_var) == false) { \
            config_var = kDefault##name;                                   \
        } else {                                                           \
            config_loaded__##name = true;                                  \
        }                                                                  \
        DUMP_KEY_AND_VALUE(name, "n/a", config_var);                       \
    };

//
// Macros for loading config with validation function
//
#define DEFINE_CONFIG_LOAD_INT_VALIDATE(name, config_var, validate_function) \
    {                                                                        \
        if (config_.GetIntValue(kConfig##name, &config_var) == true) {       \
            config_loaded__##name = true;                                    \
            if (validate_function(config_var, kDefault##name) == false) {    \
                config_var = kDefault##name;                                 \
                config_loaded__##name = false;                               \
            }                                                                \
        } else                                                               \
            config_var = kDefault##name;                                     \
        DUMP_KEY_AND_VALUE(name, "n/a", config_var);                         \
    };

#define DEFINE_CONFIG_LOAD_STR_VALIDATE(name, config_var, validate_function) \
    {                                                                        \
        if (config_.GetStringValue(kConfig##name, &config_var) == true) {    \
            config_loaded__##name = true;                                    \
            if (validate_function(config_var, kDefault##name) == false) {    \
                config_var = kDefault##name;                                 \
                config_loaded__##name = false;                               \
            }                                                                \
        } else                                                               \
            config_var = kDefault##name;                                     \
        DUMP_KEY_AND_VALUE(name, "n/a", config_var);                         \
    };

//
// Macros for loading config for config_streamer.cc
//
#define DEFINE_CONFIG_LOAD_BOOL_WITH_RETURN(name)                          \
    {                                                                      \
        std::string config_var;                                            \
        if (config_->GetStringValue(kConfig##name, &config_var) == true) { \
            config_loaded__##name = true;                                  \
            if (config_var.compare("true") == 0) {                         \
                DUMP_KEY_AND_VALUE(name, "n/a", config_var);               \
                return true;                                               \
            } else if (config_var.compare("false") == 0) {                 \
                DUMP_KEY_AND_VALUE(name, "n/a", config_var);               \
                return false;                                              \
            } else {                                                       \
                config_loaded__##name = false;                             \
                DUMP_KEY_AND_VALUE(name, "n/a", config_var);               \
                return kDefault##name;                                     \
            };                                                             \
        };                                                                 \
        DUMP_KEY_AND_VALUE(name, kDefault##name, kDefault##name);          \
        return kDefault##name;                                             \
    };

#define DEFINE_CONFIG_LOAD_INT_WITH_RETURN(name, config_var,            \
                                           validate_function)           \
    {                                                                   \
        if (config_->GetIntValue(kConfig##name, &config_var) == true) { \
            config_loaded__##name = true;                               \
            DUMP_KEY_AND_VALUE(name, "n/a", config_var);                \
            return validate_function(config_var, kDefault##name);       \
        }                                                               \
        config_loaded__##name = false;                                  \
        config_var = kDefault##name;                                    \
        DUMP_KEY_AND_VALUE(name, "n/a", config_var);                    \
        return false;                                                   \
    };

#define DEFINE_CONFIG_LOAD_STR_WITH_RETURN(name, config_var)               \
    {                                                                      \
        if (config_->GetStringValue(kConfig##name, &config_var) == true) { \
            config_loaded__##name = true;                                  \
            DUMP_KEY_AND_VALUE(name, "n/a", config_var);                   \
            return true;                                                   \
        };                                                                 \
        config_loaded__##name = false;                                     \
        config_var = kDefault##name;                                       \
        DUMP_KEY_AND_VALUE(name, "n/a", config_var);                       \
        return false;                                                      \
    };

#endif  // CONFIG_DEFINES_H_
