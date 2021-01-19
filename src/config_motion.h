/*
Copyright (c) 2021, rpi-webrtc-streamer Lyu,KeunChang

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

#ifndef CONFIG_MOTION_H_
#define CONFIG_MOTION_H_

#include <memory>

#include "compat/optionsfile.h"

class ConfigMotion {
   public:
    explicit ConfigMotion(const std::string& config_file, bool verbose = false);
    ~ConfigMotion();

    // Load Config
    bool Load();

// define expasion macros for Getter
//      type Get##name();

// define expasion macros for Getter
//      type GetName();
#define _CR(name, config_var, config_remote_access, config_type, \
            config_default)                                      \
    config_type Get##name(void) const;                           \
    config_type config_var;                                      \
    bool isloaded__##config_var;                                 \
    bool validate_value__##config_var(config_type value,         \
                                      config_type default_value);
#define _CR_B _CR
#define _CR_I _CR
#define _CR_F _CR

#include "def/config_motion.def"

    void DumpConfig(void);

   private:
    bool config_loaded_;
    std::string config_file_;
    bool dump_config_;
    std::unique_ptr<rtc::OptionsFile> options_file_;
    std::string config_dir_basename_;
};

#endif  // CONFIG_MOTION_H_
