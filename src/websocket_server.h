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

#include <algorithm>
#include <ctime>
#include <deque>
#include <list>
#include <map>
#include <string>

// __RWS_VERSION__ defined in Makefile
#define WEBSOCKET_SERVER_NAME __RWS_VERSION__

#include "websocket_handler.h"
#include "websocket_server_internal.h"

#ifndef WEBSOCKET_SERVER_H_
#define WEBSOCKET_SERVER_H_

//////////////////////////////////////////////////////////////////////
//
// Internal structure definitions
//
//////////////////////////////////////////////////////////////////////

#define MAX_URI_PATH 128

// per session stroage
struct per_session_data__libwebsockets {
    char uri_path_[MAX_URI_PATH];
    std::time_t last_ping_sent_;
};

struct vhd_libwebsocket {
    struct lws_context *context;
    struct lws_vhost *vhost;
};

struct WSInstanceContainer {
    explicit WSInstanceContainer(const int sockid, struct lws *wsi)
        : sockid_(sockid), wsi_(wsi), force_connection_drop_(false) {}
    virtual ~WSInstanceContainer() {}
    int sockid_;
    struct lws *wsi_;
    bool force_connection_drop_;
    std::string drop_message_;
    std::deque<std::string> pending_message_;
};

//////////////////////////////////////////////////////////////////////
//
// LibWebSocket
//
//////////////////////////////////////////////////////////////////////

struct WSInternalHandlerConfig : public WebSocketHandler {
    WSInternalHandlerConfig(std::string path, WebSocketHandlerType handler_type,
                            WebSocketHandler *handler)
        : path_(path), handler_type_(handler_type), handler_(handler) {}
    virtual ~WSInternalHandlerConfig() {}

    std::string path_;
    WebSocketHandlerType handler_type_;
    WebSocketHandler *handler_;
    std::list<struct WSInstanceContainer> handler_runtime_;

    virtual void OnConnect(const int sockid);
    virtual bool OnMessage(const int sockid, const std::string &message);
    virtual void OnDisconnect(const int sockid);
    virtual void OnError(const int sockid, const std::string &errmsg);

    bool CreateHandlerRuntime(const int sockid, struct lws *wsi);
    struct lws *GetWsiFromHandlerRuntime(const int sockid);
    bool QueueMessage(const int sockid, const std::string &message);
    bool DequeueMessage(const int sockid, std::string &message);
    size_t Size();
    size_t QeueueSize(const int sockid);
    bool HasPendingMessage(const int sockid);
    bool Close(int sockid, int reason_code, const std::string &message);
};

class LibWebSocketServer : public WebSocketMessage {
   public:
    enum DEBUG_LEVEL {
        DEBUG_LEVEL_ERR,
        DEBUG_LEVEL_WARN,
        DEBUG_LEVEL_NOTICE,
        DEBUG_LEVEL_INFO,
        DEBUG_LEVEL_DEBUG,
        DEBUG_LEVEL_PARSER,
        DEBUG_LEVEL_EXT,
        DEBUG_LEVEL_CLIENT,
        DEBUG_LEVEL_LATENCY,
        DEBUG_LEVEL_HEADER,
        DEBUG_LEVEL_DEFAULT,
        DEBUG_LEVEL_NONE,
        DEBUG_LEVEL_ALL
    };

    explicit LibWebSocketServer();
    ~LibWebSocketServer();

    bool AddHttpWebMount(bool motion_enabled, const std::string &web_path,
                         const std::string &motion_path);
    bool Init(int port);
    bool RunLoop(int timeout);
    void LogLevel(DEBUG_LEVEL level);
    void LogLevel(DEBUG_LEVEL level, bool log_redirect);
    static void Log(int level, const char *line);
    static int CallbackLibWebsockets(struct lws *wsi,
                                     enum lws_callback_reasons reason,
                                     void *user, void *in, size_t len);

    // Add Handler interfaces in LibWebSocket Server
    void AddWebSocketHandler(const std::string path,
                             WebSocketHandlerType instance_type,
                             WebSocketHandler *handler);

    virtual void SendMessage(int sockid, const std::string &message);
    virtual void Close(int sockid, int reason_code, const std::string &message);

   protected:
    bool IsValidWSPath(const char *path);
    bool GetFileMapping(const std::string path, std::string &file_mapping);
    WSInternalHandlerConfig *GetWebsocketHandler(const char *path);

   private:
    std::list<WSInternalHandlerConfig> wshandler_config_;

    struct lws_context_creation_info info_;
    struct lws_context *context_;
    struct lws_vhost *vhost_;
    struct lws_http_mount webroot_http_mount_;
    struct lws_http_mount motion_http_mount_;
    bool motion_mount_enabled_;
    int port_;
    DEBUG_LEVEL debug_level_;
};

#endif  // WEBSOCKET_SERVER_H_
