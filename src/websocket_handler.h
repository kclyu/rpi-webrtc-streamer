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

#include <map>
#include <memory>
#include <set>
#include <string>

#ifndef WEBSOCKET_HANDLER_H_
#define WEBSOCKET_HANDLER_H_

enum WebSocketHandlerType {
    SINGLE_INSTANCE,    // allow only one handler runtime
    MULTIPLE_INSTANCE,  // allow multiple handler runtime
};

enum FileMappingType {
    MAPPING_DEFAULT,
    MAPPING_FILE,
    MAPPING_DIRECTORY,
};

struct FileMapping {
    FileMapping(const std::string uri_prefix, FileMappingType type,
                const std::string uri_resource_path)
        : uri_prefix_(uri_prefix),
          type_(type),
          uri_resource_path_(uri_resource_path) {}
    ~FileMapping() {}
    // allowed prefix for input URI
    const std::string uri_prefix_;
    FileMappingType type_;
    // mapping local resource path for uri_prefix
    const std::string uri_resource_path_;
};

struct WebSocketHandler {
    virtual void OnConnect(int sockid) = 0;
    virtual bool OnMessage(int sockid, const std::string& message) = 0;
    virtual void OnDisconnect(int sockid) = 0;
    virtual void OnError(int sockid, const std::string& message) = 0;

   protected:
    virtual ~WebSocketHandler() {}
};

struct WebSocketMessage {
    virtual void SendMessage(int sockid, const std::string& message) = 0;
    virtual void Close(int sockid, int reason_code,
                       const std::string& message) = 0;

   protected:
    virtual ~WebSocketMessage() {}
};

#endif  // WEBSOCKET_HANDLER_H_
