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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "webrtc/base/network.h"

#include "websocket_server.h"
#include "../lib/private-libwebsockets.h"

/* http server gets files from this path */
#define LOCAL_RESOURCE_PATH INSTALL_DIR
char *resource_path = LOCAL_RESOURCE_PATH;
char crl_path[1024] = "";

//////////////////////////////////////////////////////////////////////
//
// LibWebSocket struct for initialization
//
//////////////////////////////////////////////////////////////////////
/* list of supported protocols and callbacks */
struct lws_protocols protocols[] =  {
    /* first protocol must always be HTTP handler */
    { "websocket-http",		/* name */
        LibWebSocketServer::CallbackLibWebsockets, /* callback */
        sizeof (struct per_session_data__libwebsockets), /* per_session_data_size */
        0,			/* max frame size / rx buffer */ },
    { NULL, NULL, 0, 0 } /* terminator */
};

const struct lws_extension exts[] = {
    { "permessage-deflate",
        lws_extension_callback_pm_deflate,
        "permessage-deflate" },
    { "deflate-frame",
        lws_extension_callback_pm_deflate,
        "deflate_frame" },
    { NULL, NULL, NULL /* terminator */ }
};


//////////////////////////////////////////////////////////////////////
//
// LibWebSocket Initialization
//
//////////////////////////////////////////////////////////////////////
LibWebSocketServer::LibWebSocketServer() {
    const uint32_t path_buffer_size = 1024;
    const uint32_t iface_buffer_size = 128;

	memset(&info_, 0x00, sizeof(info_));
    context_ = nullptr;
	interface_name_ = new char[iface_buffer_size];
	ms_  = oldms_ = 0;
	uid_ =  gid_ = -1;
	iface_ = nullptr;
	cert_path_ = new char[path_buffer_size];
	key_path_ = new char[path_buffer_size];
	ca_path_ = new char[path_buffer_size];

	use_ssl_ = pp_secs_= opts_ = 0;
    debug_level_ = LibWebSocketServer::DEBUG_LEVEL_NONE;
}

void LibWebSocketServer::Log(int level, const char *line) {
    const char *log_line_header="lws: ";
    char line_buffer[512];
    strcpy(line_buffer,line);
    for(size_t i = strlen(line_buffer); i > 0 ; i--) 
        if( line_buffer[i] == '\n' || line_buffer[i] == '\r' ) 
            line_buffer[i] = 0x00;

	switch (level) {
	case LLL_ERR:
		LOG(LS_ERROR) << log_line_header << line_buffer;
		break;
	case LLL_WARN:
		LOG(WARNING) << log_line_header << line_buffer;
		break;
	case LLL_NOTICE:
		LOG(INFO) << log_line_header << line_buffer;
		break;
	case LLL_INFO:
		LOG(INFO) << log_line_header << line_buffer;
		break;
	}
}

LibWebSocketServer::~LibWebSocketServer() {
	lws_context_destroy(context_);
	closelog();
    file_mapping_.clear();
    wshandler_config_.clear();
    delete interface_name_ ;
	delete cert_path_;
	delete key_path_;
	delete ca_path_;
}


void LibWebSocketServer::LogLevel(DEBUG_LEVEL level) {
    LogLevel(level,false);
}

void LibWebSocketServer::LogLevel(DEBUG_LEVEL level,bool log_redirect){
    int debug_level = 0;

    debug_level_ = level;
    switch( level ) {
        case DEBUG_LEVEL_ALL:
        case DEBUG_LEVEL_LATENCY:
            debug_level |= LLL_LATENCY;
        case DEBUG_LEVEL_CLIENT:
            debug_level |= LLL_CLIENT;
        case DEBUG_LEVEL_HEADER:
            debug_level |= LLL_HEADER;
        case DEBUG_LEVEL_EXT:
            debug_level |= LLL_EXT;
        case DEBUG_LEVEL_PARSER:
            debug_level |= LLL_PARSER;
        case DEBUG_LEVEL_DEBUG:
            debug_level |= LLL_DEBUG;
        case DEBUG_LEVEL_INFO:
            debug_level |= LLL_INFO;
        case DEBUG_LEVEL_NOTICE:
            debug_level |= LLL_NOTICE;
        case DEBUG_LEVEL_WARN:
            debug_level |= LLL_WARN;
        case DEBUG_LEVEL_ERR:
            debug_level |= LLL_ERR;
            break;
        case DEBUG_LEVEL_DEFAULT:
            debug_level = LLL_ERR | LLL_WARN | LLL_NOTICE;
            break;
        case DEBUG_LEVEL_NONE:
            debug_level = 0;    // no logging inside libwebsockets library
            break;
    }

    if(log_redirect) 
        lws_set_log_level(debug_level, this->Log);
    else 
        lws_set_log_level(debug_level, nullptr);
}


bool LibWebSocketServer::Init(int port) {
	info_.port = port_ = port;

    this->LogLevel(debug_level_);

    // keep LibWebSocketServer instance address 
    // for calling back in the user of protocol
    protocols[0].user = this;

	info_.iface = iface_;
	info_.protocols = protocols;
	info_.ssl_cert_filepath = nullptr;
	info_.ssl_private_key_filepath = nullptr;
	info_.ws_ping_pong_interval = pp_secs_;

	info_.gid =  gid_;
	info_.uid =  uid_;
	info_.max_http_header_pool = 16;
	info_.options = opts_ | LWS_SERVER_OPTION_VALIDATE_UTF8;
	info_.extensions = exts;
	info_.timeout_secs = 5;
    info_.ws_ping_pong_interval = 1;    // WebSocket Ping/Pong interval
	info_.ssl_cipher_list = "ECDHE-ECDSA-AES256-GCM-SHA384:"
			       "ECDHE-RSA-AES256-GCM-SHA384:"
			       "DHE-RSA-AES256-GCM-SHA384:"
			       "ECDHE-RSA-AES256-SHA384:"
			       "HIGH:!aNULL:!eNULL:!EXPORT:"
			       "!DES:!MD5:!PSK:!RC4:!HMAC_SHA1:"
			       "!SHA1:!DHE-RSA-AES128-GCM-SHA256:"
			       "!DHE-RSA-AES128-SHA256:"
			       "!AES128-GCM-SHA256:"
			       "!AES128-SHA256:"
			       "!DHE-RSA-AES256-SHA256:"
			       "!AES256-GCM-SHA384:"
			       "!AES256-SHA256";
    info_.server_string = WEBSOCKET_SERVER_NAME;

	context_ = lws_create_context(&info_);
	if (context_ == nullptr) {
		lwsl_err("libwebsocket init failed\n");
		return false;
	}
    return true;
}

bool LibWebSocketServer::RunLoop(int timeout) {
    const int internal_timeout_ = 25;
    if(timeout == 0) 
        timeout = internal_timeout_;
	return lws_service(context_, timeout) == 0;
}


//////////////////////////////////////////////////////////////////////
//
// FileMapping 
//
//////////////////////////////////////////////////////////////////////
void LibWebSocketServer::AddFileMapping(const std::string path, 
        FileMappingType type, const std::string map) {  
    if( type == MAPPING_DEFAULT ) { // default mapping for resource path
        map_default_resource_ = map;
        return;
    }

    for(std::list<FileMapping>::iterator iter = file_mapping_.begin(); 
            iter != file_mapping_.end(); iter++) {
        if( iter->uri_prefix_ == path ) {
            LOG(LS_ERROR) << "Do not allow same URI path in file mapping";
            return;
        }
    }
    file_mapping_.push_back(FileMapping(path,type, map));
}

bool LibWebSocketServer::GetFileMapping(const std::string path,
        std::string& file_mapping){
    // Check mapping list at first whether the request path is matching
    for(std::list<FileMapping>::iterator iter = file_mapping_.begin(); 
            iter != file_mapping_.end(); iter++) {
        if( iter->type_ == MAPPING_DIRECTORY ) {
            // compare only the uri_prefix_ size
            if( iter->uri_prefix_.compare(0, iter->uri_prefix_.size(),path)
                    == 0) {
                LOG(INFO) << "Found DIR type File Mapping URI in mapping : " << 
                    iter->uri_prefix_ << "(" << iter->uri_resource_path_  << ")";
                file_mapping = iter->uri_resource_path_ + path;
                return true;
            }
        }
        else if( iter->type_ == MAPPING_FILE ) {
            if( iter->uri_prefix_ == path ) {
                LOG(INFO) << "Found FILE type Mapping URI in mapping : " << 
                    iter->uri_prefix_ << "(" << iter->uri_resource_path_  << ")";
                file_mapping = iter->uri_resource_path_;
                return true;
            }
        }
    }

    // default mapping( resource path )
    file_mapping = map_default_resource_ + path;
    return false;
}

//////////////////////////////////////////////////////////////////////
//
// HttpHandler 
//
//////////////////////////////////////////////////////////////////////
void LibWebSocketServer::AddHttpHandler(const std::string path, 
        HttpHandler *handler) {  
    std::map<std::string,HttpHandler *>::iterator iter 
        = httphandler_config_.find(path);
    // http handler does not exist, so add it on httphandler_config_
    if( iter == httphandler_config_.end()) {
        httphandler_config_[path] = handler;
    }
}

HttpHandler *LibWebSocketServer::GetHttpHandler(const std::string path) {
    std::map<std::string,HttpHandler *>::iterator iter 
            = httphandler_config_.find(path);
    if( iter == httphandler_config_.end() )
        return nullptr;
    return iter->second;
}

//////////////////////////////////////////////////////////////////////
//
// WebSocket Handler 
//
//////////////////////////////////////////////////////////////////////
void LibWebSocketServer::AddWebSocketHandler (const std::string path, 
            WebSocketHandlerType handler_type, WebSocketHandler *handler){
    for(std::list<WSInternalHandlerConfig>::iterator iter 
            = wshandler_config_.begin(); iter != wshandler_config_.end(); iter++) {
        if( iter->path_ == path ) {
            LOG(LS_ERROR) << "Do not allow same URI path in handler config";
            return;
        }
    }
    wshandler_config_.push_back(WSInternalHandlerConfig(path,handler_type, handler));
}

WSInternalHandlerConfig *LibWebSocketServer::GetWebsocketHandler(const char *path) {
    for(std::list<WSInternalHandlerConfig>::iterator iter 
            = wshandler_config_.begin(); iter != wshandler_config_.end(); iter++) {
        if( iter->path_ == path ) {
            return &(*iter);
        }
    }
    return nullptr;
}

bool LibWebSocketServer::IsValidWSPath(const char *path) {
    for(std::list<WSInternalHandlerConfig>::iterator iter 
            = wshandler_config_.begin(); iter != wshandler_config_.end(); iter++) {
        if( iter->path_ == path ) {
            LOG(INFO) << "Found handler URI in config : " << iter->path_;
            return true;
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////
//
// SendMessage interface
//
//////////////////////////////////////////////////////////////////////
void LibWebSocketServer::SendMessage(int sockid, const std::string& message){
    for(std::list<struct WSInternalHandlerConfig>::iterator iter 
            = wshandler_config_.begin(); iter != wshandler_config_.end(); iter++) {
        if( iter->QueueMessage(sockid, message) == true ) {
            // book us a LWS_CALLBACK_HTTP_WRITEABLE callback
            lws_callback_on_writable_all_protocol(context_, 
                    &protocols[0]);

            return;
        }
    }
}

void LibWebSocketServer::Close(int sockid, int reason_code, 
        const std::string& message){
    for(std::list<struct WSInternalHandlerConfig>::iterator iter 
            = wshandler_config_.begin(); iter != wshandler_config_.end(); iter++) {

        iter->Close(sockid, reason_code, message);
        return;
    }
}


//////////////////////////////////////////////////////////////////////
//
// Websocket Internal Handler Config
//
//////////////////////////////////////////////////////////////////////
bool WSInternalHandlerConfig::CreateHandlerRuntime(const int sockid, struct lws *wsi) {
    if( handler_type_ ==  SINGLE_INSTANCE ) {
        if( handler_runtime_.size() != 0) {
            return false;
        }
    }
    handler_runtime_.push_back(WSInstanceContainer(sockid, wsi));
    return true;
}

void WSInternalHandlerConfig::OnConnect(const int sockid) {
    for(std::list<struct WSInstanceContainer>::iterator iter = handler_runtime_.begin(); 
            iter != handler_runtime_.end(); iter++) {
        if( iter->sockid_ == sockid ) {
            handler_->OnConnect(sockid);
            return;
        }
    }
    return;
}

void WSInternalHandlerConfig::OnDisconnect(const int sockid) {
    for(std::list<struct WSInstanceContainer>::iterator iter 
            = handler_runtime_.begin(); iter != handler_runtime_.end(); iter++) {

        if( iter->sockid_ == sockid ) {
            // Call OnDisconnect handler in Config
            handler_->OnDisconnect(sockid);
            handler_runtime_.erase(iter);
            return;
        };
    }
}

bool WSInternalHandlerConfig::OnMessage(const int sockid, 
        const std::string& message) {
    for(std::list<struct WSInstanceContainer>::iterator iter = handler_runtime_.begin(); 
            iter != handler_runtime_.end(); iter++) {
        if( iter->sockid_ == sockid ) {
            return handler_->OnMessage(sockid, message);
        }
    }
    return false;
}

void WSInternalHandlerConfig::OnError(const int sockid, 
        const std::string& errmsg) {
    for(std::list<struct WSInstanceContainer>::iterator iter = handler_runtime_.begin(); 
            iter != handler_runtime_.end(); iter++) {
        if( iter->sockid_ == sockid ) {
            handler_->OnError(sockid, errmsg);
        }
    }
}

bool WSInternalHandlerConfig::QueueMessage(const int sockid, 
        const std::string& message) {
    for(std::list<struct WSInstanceContainer>::iterator iter = handler_runtime_.begin(); 
            iter != handler_runtime_.end(); iter++) {
        if( iter->sockid_ == sockid ) {
            iter->pending_message_.push_back(message);
            return true;
        }
    }
    return false;
}

size_t WSInternalHandlerConfig::Size() {
    return handler_runtime_.size();
}

bool WSInternalHandlerConfig::HasPendingMessage(const int sockid) {
    for(std::list<struct WSInstanceContainer>::iterator iter = handler_runtime_.begin(); 
            iter != handler_runtime_.end(); iter++) {
        if( iter->sockid_ == sockid ) {
            return iter->pending_message_.size() > 0;
        }
    }
    // sockid is not found...
    return false;
}

bool WSInternalHandlerConfig::DequeueMessage(const int sockid,
        std::string& message) {
    for(std::list<struct WSInstanceContainer>::iterator iter = handler_runtime_.begin(); 
            iter != handler_runtime_.end(); iter++) {
        if( iter->sockid_ == sockid ) {
            if( iter->pending_message_.size() == 0 ) return false;
            message =  iter->pending_message_.front();
            iter->pending_message_.pop_front();
            return true;
        }
    }
    return false;
}

bool WSInternalHandlerConfig::Close(int sockid, int reason_code, 
        const std::string& message){
    for(std::list<struct WSInstanceContainer>::iterator iter = handler_runtime_.begin(); 
            iter != handler_runtime_.end(); iter++) {
        if( iter->sockid_ == sockid ) {
            RTC_DCHECK( iter->wsi_ != nullptr );
            lws_close_status reason_status;

            if( reason_status  == 0 ) reason_status = LWS_CLOSE_STATUS_NORMAL;
            else reason_status = (lws_close_status )reason_code;

            // TODO(kclyu) Confirmation required. Currently, 
            // lws_close_reason is considered to operate only 
            // in the receive callback handler in libwebsockets.
            lws_close_reason (iter->wsi_, reason_status, 
                    (unsigned char *)message.c_str(), message.size());

            //
            iter->force_connection_drop_ = true;
            iter->drop_message_ = message;
            return true;
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////
//
// Http Request 
//
//////////////////////////////////////////////////////////////////////
static const char *GetRequestTypeString(int type ){
    switch (type ) {
        case HTTP_GET:
            return "HTTP GET";
        case HTTP_POST:
            return "HTTP POST";
        case HTTP_PUT:
            return "HTTP PUT";
        case HTTP_DELETE:
            return "HTTP DELETE";
        case HTTP_DONTCARE:
            return "HTTP Don't Care";
        default:
            return "HTTP Unknown";
    }
}

void HttpRequest::AddHeader(int header_id, const std::string value ) {
    std::map<int,std::string>::iterator iter 
        = header_.find(header_id);

    // Do not add ARGS in the header field
    if( header_id == WSI_TOKEN_HTTP_URI_ARGS ) return;

    // header_id  does not exist, so add it on header_
    if( iter == header_.end()) {
        header_[header_id] = value;
        switch ( header_id ) {
            case WSI_TOKEN_GET_URI:
                type_ = HTTP_GET;
                break;
            case WSI_TOKEN_POST_URI:
                type_ = HTTP_POST;
                break;
            case WSI_TOKEN_PUT_URI:
                type_ = HTTP_PUT;
                break;
            case WSI_TOKEN_DELETE_URI:
                type_ = HTTP_DELETE;
                break;
        }
    }
    else {
        LOG(LS_ERROR) << "header_id(" << header_id << ") already exist.";
    }
}

void HttpRequest::AddArgs(const std::string args_param ) {
    const std::string delimiter="=";
    std::string param;
    size_t  pos;
    if((pos = args_param.find(delimiter)) != std::string::npos )  {
        param = args_param.substr(0, pos);
        args_[param] = args_param.substr(pos+delimiter.size(),std::string::npos );
    }
    else {
        LOG(LS_ERROR) << "Failed to get argument param from (" << param << ").";
    }
}

void HttpRequest::AddData(const std::string data ) {
    data_.append(data);
}

void HttpRequest::Print() {
    LOG(INFO) << "Request Type : " << GetRequestTypeString(type_);
    LOG(INFO) << "Request Headers(" << header_.size() << "):";
    for(auto t: header_) {
        LOG(INFO) << " " << t.first << ":" 
              << t.second;
    };
    LOG(INFO) << "Request Args(" << args_.size() << "):";
    for(auto t: args_) {
        LOG(INFO) << " " << t.first << ":" 
              << t.second;
    };
    if( type_ == HTTP_POST ) {
        LOG(INFO) << "Data size : " << data_.size();
        LOG(INFO) << "Data: " << data_;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Http Response 
//
//////////////////////////////////////////////////////////////////////
void HttpResponse::AddHeader(int header_id, const std::string value ) {
    std::map<int,std::string>::iterator iter 
        = header_.find(header_id);
    // header_id  does not exist, so add it on header_
    if( iter == header_.end()) {
        header_[header_id] = value;
    };
    LOG(LS_ERROR) << "header_id(" << header_id << ") already exist.";
}

void HttpResponse::SetHeaderSent() {
    header_sent_ = true;
}

bool HttpResponse::IsHeaderSent() {
    return header_sent_;
}

void HttpResponse::Print() {
    LOG(INFO) << "Response Status : " << status_;
    LOG(INFO) << "Response Header(" << header_.size() << "):";
    for(auto t: header_) {
        LOG(INFO) << " " << t.first << ":" 
              << t.second;
    };
    LOG(INFO) << "Response mine type : " << mime_;
    LOG(INFO) << "Response size : " << response_.size();
    LOG(INFO) << "Response: " << response_;
}


