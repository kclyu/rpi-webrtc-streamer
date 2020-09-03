/*
Copyright (c) 2018, rpi-webrtc-streamer Lyu,KeunChang

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

#ifndef CONFIG_MEDIA_H_
#define CONFIG_MEDIA_H_

#include <list>
#include <memory>

#include "compat/optionsfile.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/synchronization/mutex.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/******************************************************************************
 *
 * camera setting value validation defined in raspicamcontrol.c
 *
 ******************************************************************************/

int check_optionvalue_exposure_mode(const char *str);
int check_optionvalue_flicker_mode(const char *str);
int check_optionvalue_awb_mode(const char *str);
int check_optionvalue_drc_mode(const char *str);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

// The config part for global media config is required.
class ConfigMedia {
   public:
    ConfigMedia();
    ~ConfigMedia();

    struct VideoResolution {
        explicit VideoResolution(int width, int height)
            : width_(width), height_(height){};
        explicit VideoResolution(void) : width_(640), height_(480){};
        virtual ~VideoResolution(){};
        int width_;
        int height_;
    };

    bool Load(std::string config_filename);
    bool Save(void);
    bool ConfigFromJson(const std::string &config_message,
                        std::string *config_updated, std::string &error);
    bool ConfigToJson(std::string &config_message);
    void LoadConfigWithDefault(void);
    void DumpConfig(void);
    std::list<VideoResolution> GetVideoResolutionList();
    bool GetFixedVideoResolution(int &width, int &height);
    void GetMaxVideoResolution(int &width, int &height) const;

// define expasion macros for Getter
//      type GetName();
#define _CR(name, config_var, config_remote_access, config_type, \
            config_default)                                      \
    config_type Get##name(void);
#define _CR_L(name, config_var, config_remote_access, config_type, \
              config_default)
#define _CR_B _CR
#define _CR_I _CR

#include "def/config_media.def"
    // define config_media prototype
    MEDIA_CONFIG_ROW_LIST

#undef _CR
#undef _CR_L
#undef _CR_B
#undef _CR_I
#undef MEDIA_CONFIG_ROW_LIST

// define expasion macros for Setter
// bool type:
//      bool SetName(bool);
//      bool validate_value__config_var(bool,bool)
#define _CR(name, config_var, config_remote_access, config_type, \
            config_default)                                      \
    config_type config_var;                                      \
    bool isloaded__##config_var;                                 \
    bool Set##name(config_type value);                           \
    bool validate_value__##config_var(config_type value,         \
                                      config_type default_value);
#define _CR_L _CR
#define _CR_B _CR
#define _CR_I _CR

#include "def/config_media.def"
    // define config_media prototype
    MEDIA_CONFIG_ROW_LIST

#undef _CR
#undef _CR_L
#undef _CR_B
#undef _CR_I
#undef MEDIA_CONFIG_ROW_LIST

   private:
    std::unique_ptr<rtc::OptionsFile> media_optionfile_;

    webrtc::Mutex mutex_;
    std::string config_file_;
    bool config_file_loaded_;
    struct VideoResolution fixed_resolution_;
    std::list<VideoResolution> video_resolution_list_4_3_;
    std::list<VideoResolution> video_resolution_list_16_9_;

    // Additional config value validator
    bool validate__resolution(int width, int height);

    bool config_load(const std::string config_filename);

};  // ConfigMedia

class ConfigMediaSingleton {
   public:
    // Singleton, constructor and destructor are private.
    static ConfigMedia *Instance();

   private:
    ConfigMediaSingleton();
    ~ConfigMediaSingleton();

    RTC_DISALLOW_COPY_AND_ASSIGN(ConfigMediaSingleton);
};

#endif  // CONFIG_MEDIA_H_
