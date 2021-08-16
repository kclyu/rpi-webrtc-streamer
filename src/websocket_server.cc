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

#include "websocket_server.h"

#include <list>
#include <vector>

#include "absl/strings/str_format.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////
//
// LibWebSocket struct for initialization
//
///////////////////////////////////////////////////////////////////////////////

namespace {

const char *kProtocolHandlerName = "websocket-http";

const struct lws_protocol_vhost_options mjs_extension = {
    nullptr,          /* "next" pvo linked-list */
    nullptr,          /* "child" pvo linked-list */
    ".mjs",           /* file suffix to match */
    "text/javascript" /* mimetype to use */
};

const struct lws_protocol_vhost_options h264_extension = {
    &mjs_extension, /* "next" pvo linked-list */
    nullptr,        /* "child" pvo linked-list */
    ".h264",        /* file suffix to match */
    "video/h264"    /* mimetype to use */
};

/* list of supported protocols and callbacks */
struct lws_protocols protocols[] = {
    /* first protocol must always be HTTP handler */
    {
        kProtocolHandlerName,                      /* name */
        LibWebSocketServer::CallbackLibWebsockets, /* callback */
        sizeof(
            struct per_session_data__libwebsockets), /* per_session_data_size */
        4096,       /* max frame size / rx buffer */
        0, NULL, 0, /*  id, user, tx_packet_size */
    },
    {NULL, NULL, 0, 0} /* terminator */
};

const struct lws_extension extensions[] = {
    {"permessage-deflate", lws_extension_callback_pm_deflate,
     "permessage-deflate"
     "; client_no_context_takeover"
     "; client_max_window_bits"},
    {NULL, NULL, NULL /* terminator */}};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// LibWebSocket Initialization
//
////////////////////////////////////////////////////////////////////////////////
LibWebSocketServer::LibWebSocketServer() {
    memset(&info_, 0x00, sizeof(info_));
    debug_level_ = LibWebSocketServer::DEBUG_LEVEL_NONE;

    context_ = nullptr;
    vhost_ = nullptr;
    web_mounts_ = nullptr;
}

void LibWebSocketServer::Log(int level, const char *line) {
    const char *log_line_header = "lws: ";
    char line_buffer[512];
    strcpy(line_buffer, line);
    for (size_t i = strlen(line_buffer); i > 0; i--) {
        if (line_buffer[i] == '\n' || line_buffer[i] == '\r')
            line_buffer[i] = 0x00;
    }

    switch (level) {
        case LLL_ERR:
            RTC_LOG(LS_ERROR) << log_line_header << line_buffer;
            break;
        case LLL_WARN:
            RTC_LOG(WARNING) << log_line_header << line_buffer;
            break;
        case LLL_NOTICE:
            RTC_LOG(INFO) << log_line_header << line_buffer;
            break;
        case LLL_INFO:
            RTC_LOG(INFO) << log_line_header << line_buffer;
            break;
    }
}

LibWebSocketServer::~LibWebSocketServer() {
    for (lws_http_mount *http_mount : vector_http_mounts_) {
        if (http_mount->mountpoint) delete http_mount->mountpoint;
        if (http_mount->origin) delete http_mount->origin;
        if (http_mount->def) delete http_mount->def;
    }
    vector_http_mounts_.clear();
    lws_vhost_destroy(vhost_);
    lws_context_destroy(context_);
    wshandler_config_.clear();
}

void LibWebSocketServer::LogLevel(DEBUG_LEVEL level) { LogLevel(level, false); }

void LibWebSocketServer::LogLevel(DEBUG_LEVEL level, bool log_redirect) {
    int debug_level = 0;

    debug_level_ = level;
    switch (level) {
        case DEBUG_LEVEL_ALL:
            [[fallthrough]];
        case DEBUG_LEVEL_LATENCY:
            debug_level |= LLL_LATENCY;
            [[fallthrough]];
        case DEBUG_LEVEL_CLIENT:
            debug_level |= LLL_CLIENT;
            [[fallthrough]];
        case DEBUG_LEVEL_HEADER:
            debug_level |= LLL_HEADER;
            [[fallthrough]];
        case DEBUG_LEVEL_EXT:
            debug_level |= LLL_EXT;
            [[fallthrough]];
        case DEBUG_LEVEL_PARSER:
            // debug_level |= LLL_PARSER;   // supress the parser debug log
            [[fallthrough]];
        case DEBUG_LEVEL_DEBUG:
            debug_level |= LLL_DEBUG;
            [[fallthrough]];
        case DEBUG_LEVEL_INFO:
            debug_level |= LLL_INFO;
            [[fallthrough]];
        case DEBUG_LEVEL_NOTICE:
            debug_level |= LLL_NOTICE;
            [[fallthrough]];
        case DEBUG_LEVEL_WARN:
            debug_level |= LLL_WARN;
            [[fallthrough]];
        case DEBUG_LEVEL_ERR:
            debug_level |= LLL_ERR;
            break;
        case DEBUG_LEVEL_DEFAULT:
            debug_level = LLL_ERR | LLL_WARN | LLL_NOTICE;
            break;
        case DEBUG_LEVEL_NONE:
            debug_level = 0;  // no logging inside libwebsockets library
            break;
    }

    if (log_redirect)
        lws_set_log_level(debug_level, this->Log);
    else
        lws_set_log_level(debug_level, nullptr);
}

bool LibWebSocketServer::Init(int port) {
    info_.port = port_ = port;

    this->LogLevel(debug_level_);

    // keep LibWebSocketServer instance address
    // for calling back in the user of context_info
    info_.user = this;

    info_.max_http_header_pool = 16;
    info_.options =
        LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
    info_.protocols = protocols;
    info_.extensions = extensions;
    info_.timeout_secs = 10;
    info_.ws_ping_pong_interval = 1;  // WebSocket Ping/Pong interval

    info_.server_string = WEBSOCKET_SERVER_NAME;
    // need to call AddHttpWebMount before init to add web mounts
    info_.mounts = web_mounts_;

    context_ = lws_create_context(&info_);
    if (context_ == nullptr) {
        lwsl_err("failed to create context failed\n");
        return false;
    }

    vhost_ = lws_create_vhost(context_, &info_);
    if (!vhost_) {
        lwsl_err("failed to create vhost failed\n");
        return false;
    }
    return true;
}

bool LibWebSocketServer::AddHttpWebMount(const std::string &mountpoint,
                                         const std::string &mount_orig,
                                         const std::string default_file) {
    struct lws_http_mount *http_mount = nullptr;
    if (utils::IsFolder(mount_orig) == false) {
        RTC_LOG(LS_ERROR) << "Mounting oring path is not directory : "
                          << mount_orig;
        return false;
    }

    http_mount = new lws_http_mount;
    memset(http_mount, 0x00, sizeof(struct lws_http_mount));

    // make space to hold mountpoint/mount_orig/default
    char *buf = new char[mountpoint.size() + 1];
    strcpy(buf, mountpoint.c_str());
    http_mount->mountpoint = buf;
    http_mount->mountpoint_len = mountpoint.size();

    buf = new char[mount_orig.size() + 1];
    strcpy(buf, mount_orig.c_str());
    http_mount->origin = buf;

    buf = new char[default_file.size() + 1];
    strcpy(buf, default_file.c_str());
    http_mount->def = buf;

    http_mount->origin_protocol = LWSMPRO_FILE;
    // adding h.264 mine type extension
    http_mount->extra_mimetypes = &h264_extension;

    RTC_LOG(INFO) << absl::StrFormat("mount point : %s, orig: %s, default: %s",
                                     http_mount->mountpoint, http_mount->origin,
                                     http_mount->def);
    vector_http_mounts_.push_back(http_mount);
    if (web_mounts_ == nullptr) {
        web_mounts_ = http_mount;
    } else {
        http_mount->mount_next = web_mounts_;
        web_mounts_ = http_mount;
    }
    return true;
}

bool LibWebSocketServer::RunLoop(int timeout) {
    const int internal_timeout_ = 25;
    if (timeout == 0) timeout = internal_timeout_;

    return !lws_service(context_, timeout);
}

////////////////////////////////////////////////////////////////////////////////
//
// WebSocket Handler
//
////////////////////////////////////////////////////////////////////////////////
void LibWebSocketServer::AddWebSocketHandler(const std::string path,
                                             WebSocketHandlerType handler_type,
                                             WebSocketHandler *handler) {
    for (std::list<WSInternalHandlerConfig>::iterator iter =
             wshandler_config_.begin();
         iter != wshandler_config_.end(); iter++) {
        if (iter->path_ == path) {
            RTC_LOG(LS_ERROR) << "Do not allow same URI path in handler config";
            return;
        }
    }
    wshandler_config_.push_back(
        WSInternalHandlerConfig(path, handler_type, handler));
}

WSInternalHandlerConfig *LibWebSocketServer::GetWebsocketHandler(
    const char *path) {
    for (std::list<WSInternalHandlerConfig>::iterator iter =
             wshandler_config_.begin();
         iter != wshandler_config_.end(); iter++) {
        if (iter->path_ == path) {
            return &(*iter);
        }
    }
    return nullptr;
}

bool LibWebSocketServer::IsValidWSPath(const char *path) {
    for (std::list<WSInternalHandlerConfig>::iterator iter =
             wshandler_config_.begin();
         iter != wshandler_config_.end(); iter++) {
        if (iter->path_ == path) {
            RTC_LOG(INFO) << "Found handler URI in config : " << iter->path_;
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// SendMessage interface
//
////////////////////////////////////////////////////////////////////////////////
void LibWebSocketServer::SendMessage(int sockid, const std::string &message) {
    for (std::list<struct WSInternalHandlerConfig>::iterator iter =
             wshandler_config_.begin();
         iter != wshandler_config_.end(); iter++) {
        if (iter->QueueMessage(sockid, message) == true) {
            lws_callback_on_writable(iter->GetWsiFromHandlerRuntime(sockid));
            return;
        }
    }
}

void LibWebSocketServer::Close(int sockid, int reason_code,
                               const std::string &message) {
    for (std::list<struct WSInternalHandlerConfig>::iterator iter =
             wshandler_config_.begin();
         iter != wshandler_config_.end(); iter++) {
        iter->Close(sockid, reason_code, message);
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Websocket Internal Handler Config
//
///////////////////////////////////////////////////////////////////////////////
bool WSInternalHandlerConfig::CreateHandlerRuntime(const int sockid,
                                                   struct lws *wsi) {
    if (handler_type_ == MULTIPLE_INSTANCE) {
        if (handler_runtime_.size() != 0) {
            return false;
        }
    }
    handler_runtime_.push_back(WSInstanceContainer(sockid, wsi));
    return true;
}

struct lws *WSInternalHandlerConfig::GetWsiFromHandlerRuntime(
    const int sockid) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            RTC_DCHECK(iter->wsi_)
                << "The value of websocket wsi variable must not be null.";
            return iter->wsi_;
        }
    }
    return nullptr;
}

void WSInternalHandlerConfig::OnConnect(const int sockid) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            handler_->OnConnect(sockid);
            return;
        }
    }
    return;
}

void WSInternalHandlerConfig::OnDisconnect(const int sockid) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            // Call OnDisconnect handler in Config
            handler_->OnDisconnect(sockid);
            handler_runtime_.erase(iter);
            return;
        };
    }
}

bool WSInternalHandlerConfig::OnMessage(const int sockid,
                                        const std::string &message) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            return handler_->OnMessage(sockid, message);
        }
    }
    return false;
}

void WSInternalHandlerConfig::OnError(const int sockid,
                                      const std::string &errmsg) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            handler_->OnError(sockid, errmsg);
        }
    }
}

bool WSInternalHandlerConfig::QueueMessage(const int sockid,
                                           const std::string &message) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            iter->pending_message_.push_back(message);
            return true;
        }
    }
    return false;
}

size_t WSInternalHandlerConfig::Size() { return handler_runtime_.size(); }

size_t WSInternalHandlerConfig::QeueueSize(const int sockid) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            return iter->pending_message_.size();
        }
    }
    return 0;
}

bool WSInternalHandlerConfig::HasPendingMessage(const int sockid) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            return iter->pending_message_.size() > 0;
        }
    }
    // sockid is not found...
    return false;
}

bool WSInternalHandlerConfig::DequeueMessage(const int sockid,
                                             std::string &message) {
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            if (iter->pending_message_.size() == 0) {
                return false;
            }
            message = iter->pending_message_.front();
            iter->pending_message_.pop_front();
            return true;
        }
    }
    return false;
}

bool WSInternalHandlerConfig::Close(int sockid, int reason_code,
                                    const std::string &message) {
    RTC_LOG(INFO) << "Websocket Server Closing socket " << sockid;
    for (std::list<struct WSInstanceContainer>::iterator iter =
             handler_runtime_.begin();
         iter != handler_runtime_.end(); iter++) {
        if (iter->sockid_ == sockid) {
            RTC_DCHECK(iter->wsi_ != nullptr);
            lws_close_status reason_status;

            if (reason_code == 0)
                reason_status = LWS_CLOSE_STATUS_NORMAL;
            else
                reason_status = (lws_close_status)reason_code;

            // TODO(kclyu) Confirmation required. Currently,
            // lws_close_reason is considered to operate only
            // in the receive callback handler in libwebsockets.
            lws_close_reason(iter->wsi_, reason_status,
                             (unsigned char *)message.c_str(), message.size());

            //
            iter->force_connection_drop_ = true;
            iter->drop_message_ = message;
            return true;
        }
    }
    return false;
}
