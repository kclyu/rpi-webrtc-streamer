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

#include <memory>
#include <string>
#include <list>
#include <deque>
#include <map>
#include <algorithm>

#include "websocket_server_internal.h"
#include "websocket_handler.h"

#ifndef WEBSOCKET_SERVER_H_
#define WEBSOCKET_SERVER_H_

#define  MAX_URI_PATH 128
#define  MAX_RINGBUFFER_QUEUE 4
#define  MAX_SENDBUFFER_SIZE  4096

// __RWS_VERSION__ defined in Makefile
#define WEBSOCKET_SERVER_NAME __RWS_VERSION__

struct message_buf {
	char *payload;
	size_t len;
};

#define USERDATA_INITED         1  
#define USERDATA_UNINITED       0 


// UserData Struct per session
struct per_session_data__libwebsockets {
	int userdata_inited;
    char uri_path[MAX_URI_PATH];
	int ring_buffer_index;
    struct message_buf ring_buffer[MAX_RINGBUFFER_QUEUE];
    struct HttpRequest *http_request;
    struct HttpResponse *http_response;
};

struct WSInstanceContainer {
    explicit WSInstanceContainer(const int sockid,struct lws* wsi )
        : sockid_(sockid), wsi_(wsi), force_connection_drop_(false){}
    virtual ~WSInstanceContainer() {}
    int sockid_;
    struct lws *wsi_;
    bool force_connection_drop_;
    std::string drop_message_;
    std::deque<std::string> pending_message_;
};

struct WSInternalHandlerConfig : public WebSocketHandler {
    WSInternalHandlerConfig(std::string path, WebSocketHandlerType handler_type, 
            WebSocketHandler *handler ) :
        path_(path), handler_type_(handler_type), handler_(handler) {}
    virtual ~WSInternalHandlerConfig() {}

    std::string path_;
    WebSocketHandlerType handler_type_;
    WebSocketHandler *handler_;
    std::list<struct WSInstanceContainer> handler_runtime_;

    virtual void OnConnect(const int sockid);
    virtual bool OnMessage(const int sockid, const std::string& message);
    virtual void OnDisconnect(const int sockid);
    virtual void OnError(const int sockid, const std::string& errmsg);

    bool CreateHandlerRuntime(const int sockid, struct lws *wsi);
    bool QueueMessage(const int sockid, const std::string& message);
    bool DequeueMessage(const int sockid, std::string& message);
    size_t Size();
    bool HasPendingMessage(const int sockid);
    bool Close(int sockid, int reason_code, const std::string& message);
};

class LibWebSocketServer: public WebSocketMessage {
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

    bool Init(int port);
    bool RunLoop(int timeout);
    void LogLevel(DEBUG_LEVEL level);
    void LogLevel(DEBUG_LEVEL level,bool log_redirect);
    static void Log (int level, const char *line);
    static int  CallbackLibWebsockets(struct lws *wsi, enum lws_callback_reasons reason, 
            void *user, void *in, size_t len);

    // Add Handler interfaces in LibWebSocket Server
    void AddFileMapping(const std::string path, 
            FileMappingType type, const std::string map);
    void AddHttpHandler(const std::string path, HttpHandler *handler);
    void AddWebSocketHandler(const std::string path, 
            WebSocketHandlerType instance_type, WebSocketHandler *handler);

    virtual void SendMessage(int sockid, const std::string& message);
    virtual void Close(int sockid, int reason_code, const std::string& message);

protected:
    bool IsValidWSPath(const char *path);
    bool GetFileMapping(const std::string path,std::string& file_mapping);
    WSInternalHandlerConfig *GetWebsocketHandler(const char *path);
    HttpHandler *GetHttpHandler(const std::string path);

private:
    std::string map_default_resource_;
    std::list<FileMapping> file_mapping_;
    std::list<WSInternalHandlerConfig> wshandler_config_;
    std::map<std::string, struct HttpHandler *> httphandler_config_;

    struct lws_context_creation_info info_;
    struct lws_context *context_;
	char *interface_name_;
	unsigned int ms_, oldms_ ;
	const char *iface_;
	char *cert_path_;
	char *key_path_;
	char *ca_path_;
	int uid_, gid_;
	int use_ssl_;
	int pp_secs_;
	int opts_;
    int port_;
    DEBUG_LEVEL debug_level_;
};

#endif // WEBSOCKET_SERVER_H_

