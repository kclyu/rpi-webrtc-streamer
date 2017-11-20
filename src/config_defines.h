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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CONFIG_DEFINES_H_
#define CONFIG_DEFINES_H_
#include <string>

#include "rtc_base/optionsfile.h"
#include "rtc_base/stringencode.h"


///////////////////////////////////////////////////////////////////////////////////////////
//
// macro to define configuration
//
///////////////////////////////////////////////////////////////////////////////////////////
#define CONFIG_DEFINE(name, config, config_type, default_value) \
    static const char kConfig## name[] = #config ; \
    static const config_type kDefault## name = default_value ; \
    config_type config = default_value; 

#define CONFIG_LOAD_BOOL(key,value) \
        { \
            std::string flag_value; \
            if( config_.GetStringValue(kConfig ## key, &flag_value ) == true){ \
                if(flag_value.compare("true") == 0) value = true; \
                else if (flag_value.compare("false") == 0) value = false; \
                else RTC_LOG(INFO) << "Default Config \"" <<  kConfig ## key \
                        << "\" value is not valid" << flag_value; \
            }; \
        };

#define CONFIG_LOAD_BOOL_WITH_DEFAULT(key,value,default_value) \
        { \
            std::string flag_value; \
            if( config_.GetStringValue(kConfig ## key, &flag_value ) == true){ \
                if(flag_value.compare("true") == 0) value = true; \
                else if (flag_value.compare("false") == 0) value = false; \
                else { \
                    RTC_LOG(INFO) << "Default Config \"" <<  kConfig ## key \
                        << "\" value is not valid" << flag_value; \
                    value = default_value; \
                };  \
            }; \
        };

#define CONFIG_LOAD_INT_WITH_DEFAULT(key,value,validate_function,default_value) \
        { \
            if( config_.GetIntValue(kConfig ## key, &value ) == true){ \
                validate_function(value, default_value); \
            } \
            else value = default_value; \
        };

///////////////////////////////////////////////////////////////////////////////////////////
//
// macro to load configuration
//
///////////////////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_LOAD_DUMP 
    #define DUMP_KEY_AND_VALUE(dkey,dconf,dvalue)  \
        RTC_LOG(INFO) << "Config Key \"" <<  dkey << "\" Setting : \"" \
            << dconf << "\" Loaded Value : \"" << dvalue << "\""; 
#else
    #define DUMP_KEY_AND_VALUE(dkey,dconf,dvalue) 
#endif 


#define DEFINE_CONFIG_LOAD_BOOL(key,value) \
        { \
            std::string flag_value; \
            if( config_.GetStringValue(kConfig ## key, &flag_value ) == true){ \
                if(flag_value.compare("true") == 0) value = true; \
                else if (flag_value.compare("false") == 0) value = false; \
                else { \
                    RTC_LOG(INFO) << "Default Config \"" <<  kConfig ## key \
                        << "\" value is not valid" << flag_value; \
                    value = kDefault ## key; \
                };  \
            }; \
            DUMP_KEY_AND_VALUE( kConfig ## key, flag_value, value ); \
        };


#define DEFINE_CONFIG_LOAD_INT(key,value) \
        { \
            if( config_.GetIntValue(kConfig ## key, &value ) == false){ \
                value = kDefault ## key; \
            } \
            DUMP_KEY_AND_VALUE( kConfig ## key, "n/a", value ); \
        };

#define DEFINE_CONFIG_LOAD_FLOAT(key,value) \
        { \
            std::string flag_value; \
            if( config_.GetStringValue(kConfig ## key, &flag_value ) == true){ \
                if( rtc::FromString( flag_value, &value ) == false ) { \
                    RTC_LOG(INFO) << "Invalid config \"" << kConfig ## key \
                        << "\", value : " << flag_value ; \
                    RTC_LOG(INFO) << "Default Config \"" <<  kConfig ## key \
                        << "\" value is not valid" << flag_value; \
                    value = kDefault ## key; \
                } \
            } \
            DUMP_KEY_AND_VALUE( kConfig ## key, flag_value, value ); \
        };

#define DEFINE_CONFIG_LOAD_STR(key,value ) \
        { \
            if( config_.GetStringValue(kConfig ## key, &value ) == false){ \
                value = kDefault ## key; \
            }; \
            DUMP_KEY_AND_VALUE( kConfig ## key, "n/a", value ); \
        };

#define DEFINE_CONFIG_LOAD_INT_VALIDATE(key,value,validate_function) \
        { \
            if( config_.GetIntValue(kConfig ## key, &value ) == true){ \
                validate_function(value, kDefault ## key ); \
            } \
            else value = kDefault ## key; \
            DUMP_KEY_AND_VALUE( kConfig ## key, "n/a", value ); \
        };

#define DEFINE_CONFIG_LOAD_STR_VALIDATE(key,value,validate_function) \
        { \
            if( config_.GetStringValue(kConfig ## key, &value ) == true){ \
                validate_function(value, kDefault ## key ); \
            } \
            else value = kDefault ## key; \
            DUMP_KEY_AND_VALUE( kConfig ## key, "n/a", value ); \
        };

#endif  // CONFIG_DEFINES_H_


