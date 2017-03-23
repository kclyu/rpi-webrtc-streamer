/*
 *  Copyright (c) 2016, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * Modified version of webrtc/src/webrtc/examples/peer/server/data_socket.cc in WebRTC source tree
 * The origianl copyright info below.
 */
/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(WEBRTC_POSIX)
#include <unistd.h>
#endif
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/socket.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/stringencode.h"

#include "stream_data_sockets.h"

static const char kHeaderTerminator[] = "\r\n\r\n";
static const int kHeaderTerminatorLength = sizeof(kHeaderTerminator) - 1;

static const char kCrossOriginAllowHeaders[] =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Credentials: true\r\n"
    "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type, "
    "Content-Length, Connection, Cache-Control\r\n"
    "Access-Control-Expose-Headers: Content-Length, X-Peer-Id\r\n";

static const size_t kMaxNameLength = 512;
static const char kPeerIdHeader[] = "Pragma: ";
static const char kPeerId[] = "peer_id=";
static const char kTargetPeerIdParam[] = "to=";

using rtc::sprintfn;

static std::string get_extra_idheaders(const int peer_id) {
    std::string extra_headers(kPeerIdHeader + int2str(peer_id) + "\r\n");
    return extra_headers;
}

////////////////////////////////////////////////////////////////////////////////
// PeerId
////////////////////////////////////////////////////////////////////////////////
int PeerId::peer_id_ = 2;			//  peer_id 1 is streamer object
int PeerId::GeneratePeerId() {
    return peer_id_ ++;
}
////////////////////////////////////////////////////////////////////////////////
// StreamSession
////////////////////////////////////////////////////////////////////////////////

StreamSession* StreamSession::stream_session_ = nullptr;
StreamSession *StreamSession::GetInstance() {
    if(stream_session_ == nullptr) {
        stream_session_ = new StreamSession();
        stream_session_->active_session_entry_ = nullptr;
        stream_session_->active_peer_id_ = 0;
        stream_session_->streamer_bridge_ = StreamerBridge::GetInstance();
    }
    return stream_session_;
}

int StreamSession::CreateSessionEntry(void) {
    LOG(INFO) << __FUNCTION__;
    int peer_id = PeerId::GeneratePeerId();
    RTC_DCHECK(peer_id > 0);

    StreamSessionEntry *se = new StreamSessionEntry();

    se->peer_id_ = peer_id;
    se->status_ = STATUS_INVALID;
    se->timestamp_ms_ = rtc::TimeMillis();
    se->queue_timestamp_ms_ = rtc::TimeMillis();
    session_holder_.push_back(se);

    return peer_id;
};


void StreamSession::DestroySessionEntry(int peer_id) {
    StreamSessionEntry *entry = GetSessionEntry(peer_id);
    auto it = std::find(session_holder_.begin(), session_holder_.end(), entry);
    if(it != session_holder_.end())
        session_holder_.erase(it);
    delete entry;
}

size_t StreamSession::GetSessionSize(void) {
    return session_holder_.size();
}

StreamSessionEntry * StreamSession::GetSessionEntry(int peer_id) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK( peer_id != 0 );
    for (std::vector<struct StreamSessionEntry *>::const_iterator it = session_holder_.begin();
            it != session_holder_.end(); ++it) {
        StreamSessionEntry *se = *it;
        LOG(INFO) << "Search Session " << se << ", id: " << se->peer_id_ << ", target: " << peer_id;
        if( (*it)->peer_id_ == peer_id ) return *it;
    }
    return nullptr;
}

void StreamSession::ConnectSignalEvent(StreamConnection *sc) {
    LOG(INFO) << __FUNCTION__;
    SignalEvent_.connect(sc, &StreamConnection::OnEvent );
}

void StreamSession::SignalEvent(int peer_id, bool target, QueuedResponse &resp) {
    LOG(INFO) << __FUNCTION__;
    SignalEvent_(peer_id, target, resp);
}

void StreamSession::RegisterObserver(StreamerObserver* callback) {
    LOG(INFO) << __FUNCTION__;
}


// TODO auth code validation
bool StreamSession::ActivateStreamerSession(const int peer_id) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK( streamer_bridge_ != nullptr );
    StreamSessionEntry *entry = GetSessionEntry(peer_id);

    // streamer session is not active
    if( active_peer_id_ == 0 ) {
        entry->status_ = STATUS_ACTIVE;
        entry->timestamp_ms_ = rtc::TimeMillis();
        entry->queue_timestamp_ms_ = rtc::TimeMillis();

        active_peer_id_ = peer_id;
        active_session_entry_ = entry;

        streamer_bridge_->ObtainStreamer(this, peer_id, entry->name_ );
        return true;
    }
    else { // streamer session occupied by another session id
        if( active_peer_id_ != peer_id ) {
            entry->status_ = STATUS_INACTIVE;
        }
        entry->timestamp_ms_ = rtc::TimeMillis();
    }

    return false;
};

// TODO auth code validation
void StreamSession::DeactivateStreamerSession(const int peer_id) {
    LOG(INFO) << __FUNCTION__;
    if( peer_id == active_peer_id_ )  {
        StreamSessionEntry *entry = GetSessionEntry(peer_id);
        entry->status_ = STATUS_INACTIVE;
        entry->timestamp_ms_ = rtc::TimeMillis();
        entry->queue_timestamp_ms_ = rtc::TimeMillis();

        active_peer_id_ = 0;
        active_session_entry_ = nullptr;

        streamer_bridge_->ReleaseStreamer(this, peer_id);
    }
}


bool StreamSession::SendMessageToPeer(int peer_id, const std::string &message) {
    LOG(INFO) << __FUNCTION__;
    if(active_peer_id_ != 0 ) {
        StreamSessionEntry *entry = GetSessionEntry(active_peer_id_);
        if( entry == nullptr ) {
            LOG(LS_ERROR) << "Invalid stream session peer id " << active_peer_id_;
            return false;
        }
        RTC_DCHECK( entry->peer_id_ == peer_id );
        RTC_DCHECK( entry->peer_id_ == active_peer_id_ );
        RTC_DCHECK( entry == active_session_entry_ );

        // Sending 200 OK to waiting socket
        QueuedResponse resp;
        resp.status = "200 OK";
        resp.content_type = "text/plain";
        //  peer_id 1 is streamer instance
        resp.extra_headers = get_extra_idheaders(1);
        resp.data = message;

        SignalEvent(peer_id, true, resp);
        return true;
    }
    LOG(LS_ERROR) << "Stream Session is not configure to negotiate.";
    return false;
}

bool StreamSession::ReceiveMessageFromPeer(int peer_id, const std::string &message) {
    LOG(INFO) << __FUNCTION__;
    if(active_peer_id_ != 0 ) {
        StreamSessionEntry *entry = GetSessionEntry(active_peer_id_);
        if( entry == nullptr ) {
            LOG(LS_ERROR) << "Invalid stream session peer id " << active_peer_id_;
            return false;
        }
        RTC_DCHECK( entry->peer_id_ == peer_id );
        RTC_DCHECK( entry->peer_id_ == active_peer_id_ );
        RTC_DCHECK( entry == active_session_entry_ );

        streamer_bridge_->MessageFromPeer(peer_id, message);

        return true;
    }
    LOG(LS_ERROR) << "Stream Session is not configure to negotiate.";
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// StreamSessionEntry
////////////////////////////////////////////////////////////////////////////////

// Returns a string in the form "name,id,connected\n".
std::string StreamSessionEntry::GetEntry() const {
    LOG(INFO) << __FUNCTION__;
    RTC_CHECK(name_.length() <= kMaxNameLength);

    // name, 11-digit int, 1-digit bool, newline, null
    char entry[kMaxNameLength + 15];
    sprintfn(entry, sizeof(entry), "%s,%d,%d\n",
             name_.substr(0, kMaxNameLength).c_str(), peer_id_, 1);
    return entry;
}

std::string StreamSessionEntry::GetPeerIdHeader() const {
    std::string ret(kPeerIdHeader + int2str(peer_id_) + "\r\n");
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// StreamConnections
////////////////////////////////////////////////////////////////////////////////
StreamConnection::StreamConnection(rtc::AsyncSocket* socket) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK( socket != NULL );

    socket_ = socket;
    peer_id_ = 0;
    method_ = METHOD_INVALID;
    timestamp_ms_ = 0;
    content_length_ = 0;
    waiting_socket_ = false;
}

StreamConnection::~StreamConnection() {
}


void StreamConnection::DumpStreamConnection( void ) {
    LOG(INFO) << __FUNCTION__;
    if( socket_ != nullptr )  {
        LOG(INFO) << "AsocketSocket : " << socket_
                  << ", Local: " << socket_->GetLocalAddress()
                  << ", Remote: " << socket_->GetRemoteAddress();
        LOG(INFO) << " - Method : " << str_stream_connection_method(method_);
        LOG(INFO) << " - Request (" << content_length_ << "), type: " << content_type_
                  << ", path: " << request_path_;
        LOG(INFO) << " - header: " <<  request_headers_;
        LOG(INFO) << " - data: " <<  data_;
    }
    else {
        LOG(INFO) << "AsyncSocket is NULL";
    }
};

std::string StreamConnection::request_arguments(void) const {
    size_t args = request_path_.find('?');
    if (args != std::string::npos)
        return request_path_.substr(args + 1);
    return "";
}

bool StreamConnection::PathEquals(const char* path) const {
    RTC_DCHECK(path);
    size_t args = request_path_.find('?');
    if (args != std::string::npos)
        return request_path_.substr(0, args).compare(path) == 0;
    return request_path_.compare(path) == 0;
}

bool StreamConnection::Match(rtc::AsyncSocket* socket) {
    return (socket == socket_) ? true : false;
}

void StreamConnection::Clear(void) {
    method_ = METHOD_INVALID;
    content_length_ = 0;
    content_type_.clear();
    request_path_.clear();
    request_headers_.clear();
    data_.clear();
}

bool StreamConnection::ParseHeaders(void) {
    RTC_DCHECK(!request_headers_.empty());
    RTC_DCHECK(method_ == METHOD_INVALID);
    size_t i = request_headers_.find("\r\n");
    if (i == std::string::npos)
        return false;

    if (!ParseMethodAndPath(request_headers_.data(), i))
        return false;

    RTC_DCHECK(method_ != METHOD_INVALID);
    RTC_DCHECK(!request_path_.empty());

    if (method_ == METHOD_POST) {
        const char* headers = request_headers_.data() + i + 2;
        size_t len = request_headers_.length() - i - 2;
        if (!ParseContentLengthAndType(headers, len))
            return false;
    }

    return true;
}

bool StreamConnection::ParseMethodAndPath(const char* begin, size_t len) {
    struct {
        const char* method_name;
        size_t method_name_len;
        RequestMethod id;
    } supported_methods[] = {
        { "GET", 3, METHOD_GET },
        { "POST", 4, METHOD_POST },
        { "OPTIONS", 7, METHOD_OPTIONS },
    };

    const char* path = NULL;
    for (size_t i = 0; i < ARRAYSIZE(supported_methods); ++i) {
        if (len > supported_methods[i].method_name_len &&
                isspace(begin[supported_methods[i].method_name_len]) &&
                strncmp(begin, supported_methods[i].method_name,
                        supported_methods[i].method_name_len) == 0) {
            method_ = supported_methods[i].id;
            path = begin + supported_methods[i].method_name_len;
            break;
        }
    }

    const char* end = begin + len;
    if (!path || path >= end)
        return false;

    ++path;
    begin = path;
    while (!isspace(*path) && path < end)
        ++path;

    request_path_.assign(begin, path - begin);

    return true;
}


bool StreamConnection::ParseContentLengthAndType(const char* headers, size_t length) {
    RTC_DCHECK(content_length_ == 0);
    RTC_DCHECK(content_type_.empty());

    const char* end = headers + length;
    while (headers && headers < end) {
        if (!isspace(headers[0])) {
            static const char kContentLength[] = "Content-Length:";
            static const char kContentType[] = "Content-Type:";
            if ((headers + ARRAYSIZE(kContentLength)) < end &&
                    strncmp(headers, kContentLength,
                            ARRAYSIZE(kContentLength) - 1) == 0) {
                headers += ARRAYSIZE(kContentLength) - 1;
                while (headers[0] == ' ')
                    ++headers;
                content_length_ = atoi(headers);
            } else if ((headers + ARRAYSIZE(kContentType)) < end &&
                       strncmp(headers, kContentType,
                               ARRAYSIZE(kContentType) - 1) == 0) {
                headers += ARRAYSIZE(kContentType) - 1;
                while (headers[0] == ' ')
                    ++headers;
                const char* type_end = strstr(headers, "\r\n");
                if (type_end == NULL)
                    type_end = end;
                content_type_.assign(headers, type_end);
            }
        } else {
            ++headers;
        }
        headers = strstr(headers, "\r\n");
        if (headers)
            headers += 2;
    }

    return !content_type_.empty() && content_length_ != 0;
}


bool StreamConnection::Send(const std::string& data) const {
    LOG(INFO) << "Sending : " << data;
    return (socket_->Send(data.data(), data.length()) != SOCKET_ERROR);
}

bool StreamConnection::Send(const std::string& status, bool connection_close,
                            const std::string& content_type,
                            const std::string& extra_headers,
                            const std::string& data) const {
    //RTC_DCHECK(Valid());
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(!status.empty());
    std::string buffer("HTTP/1.1 " + status + "\r\n");

    buffer += "Server: PeerConnectionTestServer/0.1\r\n"
              "Cache-Control: no-cache\r\n";

    if (connection_close)
        buffer += "Connection: close\r\n";

    if (!content_type.empty())
        buffer += "Content-Type: " + content_type + "\r\n";

    buffer += "Content-Length: " + int2str(static_cast<int>(data.size())) +
              "\r\n";

    if (!extra_headers.empty()) {
        buffer += extra_headers;
        // Extra headers are assumed to have a separator per header.
    }

    buffer += kCrossOriginAllowHeaders;

    buffer += "\r\n";
    buffer += data;

    return Send(buffer);
}

bool StreamConnection::Valid() const {
    return (socket_->GetState() == rtc::AsyncSocket::CS_CONNECTED );
}

bool StreamConnection::HandleReceivedData(char *data, int length) {
    LOG(INFO) << __FUNCTION__;

    bool ret = true;
    if (headers_received()) {
        if (method_ != METHOD_POST) {
            // unexpectedly received data.
            ret = false;
        } else {
            data_.append(data, length);
        }
    } else {
        request_headers_.append(data, length);
        size_t found = request_headers_.find(kHeaderTerminator);
        if (found != std::string::npos) {
            data_ = request_headers_.substr(found + kHeaderTerminatorLength);
            request_headers_.resize(found + kHeaderTerminatorLength);
            ret = ParseHeaders();
        }
    }
    return ret;
}

//
bool StreamConnection::HandleRequest( void ) {
    LOG(INFO) << __FUNCTION__;
    std::string name;
    StreamSession* session = StreamSession::GetInstance();

    if( method_ == METHOD_GET  && PathEquals("/sign_in") ) {
        LOG(INFO) << "sign_in request processing. ";

        int peer_id = 0;
        std::string name;

        // Create and initialize the session entry
        peer_id = session->CreateSessionEntry();
        RTC_DCHECK(peer_id != 0);

        name = rtc::s_url_decode(request_arguments());
        if (name.empty())
            name = "peer_" + int2str(peer_id);
        else if (name.length() > kMaxNameLength)
            name.resize(kMaxNameLength);
        std::replace(name.begin(), name.end(), ',', '_');

        StreamSessionEntry *entry = session->GetSessionEntry(peer_id);
        entry->name_ = name;

        LOG(INFO) << "New session member added " << name << ", peer_id : " << peer_id;
        std::string content_type = "text/plain";
        std::string response(entry->GetEntry() + "RaspiStreamer,1,1\r\n");
        Send("200 Added", true, content_type, entry->GetPeerIdHeader(),
             response);
        return true;

    }

    //  to process following request peer_id session must exist,
    //    : sign_out, wait, message
    //
    std::string args(request_arguments());
    size_t found = args.find(kPeerId);
    if (found == std::string::npos) {
        LOG(LS_ERROR) << "invalid request(" << request_path_ << ") peer_id missing. ";
        Send("400 OK", true, "text/plain", "", "peer_id argument is missing.");
        return false;
    }

    peer_id_ = atoi(&args[found + ARRAYSIZE(kPeerId) - 1]);
    StreamSessionEntry *entry = session->GetSessionEntry(peer_id_);
    if( entry == nullptr ) {
        LOG(LS_ERROR) << "peer_id " << peer_id_ << " session does not exist: ";
        Send("400 OK", true, "text/plain", "", "peer id session does not exists");
        return false;
    };


    if( method_ == METHOD_GET  && PathEquals("/sign_out") ) {
        LOG(INFO) << "sign_out request processing. peer_id: " << peer_id_;
        session->DeactivateStreamerSession(peer_id_);

        // Sending 200 OK to waiting socket
        QueuedResponse resp;
        resp.status = "200 OK";
        resp.content_type = "text/plain";
        resp.extra_headers = "";
        resp.data = "";
        session->SignalEvent(peer_id_, false, resp);

        session->DestroySessionEntry(peer_id_);
        LOG(INFO) << "peer_id " << peer_id_
                  << " session destroyed(left:" << session->GetSessionSize() << ").";

        // Sending the sign_out response
        Send("200 OK", true, "text/plain", "", "");
        return true;
    }
    if( method_ == METHOD_GET  && PathEquals("/wait") ) {
        LOG(INFO) << "wait request processing. ";
        session->ActivateStreamerSession(peer_id_);
        if( entry->queue_.size() )  {
            const QueuedResponse& response = entry->queue_.front();
            Send(response.status, true, response.content_type,
                 response.extra_headers, response.data);
            entry->queue_.pop();
        }
        else {
            waiting_socket_ = true;
            session->ConnectSignalEvent(this);
        }
        return true;
    }
    if( (method_ == METHOD_POST||method_ == METHOD_GET)  && PathEquals("/message") ) {
        LOG(INFO) << "message sending request processing. ";
        std::string args(request_arguments());
        size_t found = args.find(kTargetPeerIdParam);
        if (found == std::string::npos) {
            LOG(LS_ERROR) << "invalid request(" << request_path_ << ") target id missing. ";
            Send("400 OK", true, "text/plain", "", "target id argument is missing.");
            return false;
        }

        int target_peer_id = atoi(&args[found + ARRAYSIZE(kTargetPeerIdParam) - 1]);
        // target peer id must be 1
        if(target_peer_id != 1 ) {
            LOG(LS_ERROR) << "invalid target id(" <<  target_peer_id << ")";
            Send("400 OK", true, "text/plain", "", "invalid target id argument.");
            return false;
        }

        LOG(INFO) << "message target id : " << target_peer_id;
        session->ReceiveMessageFromPeer( peer_id_, data_ );

        // Sending the sign_out response
        Send("200 OK", true, "text/plain", "", "");
        return true;
    }
    return false;
}

void StreamConnection::OnEvent(int peer_id, bool target, QueuedResponse &resp) {
    LOG(INFO) << __FUNCTION__ ;
    RTC_DCHECK(waiting_socket_);

    // this event message is not for this peer_id
    if( target == true && peer_id_ != peer_id ) return;
    if(Send(resp.status, true, resp.content_type, resp.extra_headers,resp.data) <= 0 ) {
        // Queue the response
        StreamSessionEntry *entry = StreamSession::GetInstance()->GetSessionEntry(peer_id);
        entry->queue_.push(resp);
    }
}


