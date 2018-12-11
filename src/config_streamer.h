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

#ifndef RPI_STREAMER_CONFIG_
#define RPI_STREAMER_CONFIG_

#include "rtc_base/checks.h"

#include "api/peerconnectioninterface.h"

#include "compat/optionsfile.h"

class StreamerConfig  {
public:
    explicit StreamerConfig(const std::string &config_file);
    ~StreamerConfig();

    // Load Config
    bool LoadConfig();
    const std::string GetConfigFilename();

    bool GetDisableLogBuffering();
    bool GetLibwebsocketDebugEnable();
    bool GetWebRootPath(std::string& path);
    bool GetRwsWsURL(std::string& path);
    bool GetWebSocketEnable();
    bool GetWebSocketPort(int& port);

    bool GetDirectSocketEnable();
    bool GetDirectSocketPort(int& port);

    bool GetStunServer(webrtc::PeerConnectionInterface::IceServer &server);
    bool GetTurnServer(webrtc::PeerConnectionInterface::IceServer &server);

    bool GetAudioEnable();
    bool GetVideoEnable();

    bool GetMediaConfig(std::string& conf);
    bool GetMotionConfig(std::string& conf);
    bool GetLogPath(std::string& log_dir);

private:
    bool config_loaded_;
    std::unique_ptr<rtc::OptionsFile> config_;
    std::string config_file_;
    std::string config_dir_basename_;
};



#endif  // RPI_STREAMER_CONFIG_
