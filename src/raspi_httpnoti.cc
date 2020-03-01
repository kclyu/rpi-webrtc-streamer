/*
 * Copyright (c) 2018, rpi-webrtc-streamer  Lyu,KeunChang
 *
 * raspi_httpnoti.cc
 *
 * Modified version of src/webrtc/examples/peer/client/peer_connection_client.cc
 * in WebRTC source tree. The origianl copyright info below.
 */
/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "raspi_httpnoti.h"

#include <limits>
#include <list>
#include <string>

#include "common_types.h"
#include "rtc_base/checks.h"
#include "rtc_base/fileutils.h"
#include "rtc_base/httpcommon.h"
#include "rtc_base/logging.h"
#include "rtc_base/stream.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"
#include "utils.h"  // GetHardwareDeviceId
#include "utils_url.h"

// Do not allow buffer sizes larger than the specified max value.
static const int kMaxBufferSize = 4096;
static const int kDelaySendActivate = 1000;
static const int kDelayResendActivate = 3000;
static const int kDelayResolveIfAddr = 5000;  // 5 seconds

static const char* kDefaultMotionNotiIP = "localhost";
static const char* kDefaultMotionNotiPath = "/motion_notification";
static const int kDefaultMotionNotiPort = 8890;
static const int kDefaultMotionFileServingPort = 8889;

rtc::AsyncSocket* RaspiHttpNoti::CreateClientSocket(int family) {
    rtc::Thread* thread = rtc::Thread::Current();
    RTC_DCHECK(thread != nullptr);
    return thread->socketserver()->CreateAsyncSocket(family, SOCK_STREAM);
}

RaspiHttpNoti::RaspiHttpNoti()
    : server_(kDefaultMotionNotiIP),
      port_(kDefaultMotionNotiPort),
      path_(kDefaultMotionNotiPath),
      state_(HTTPNOTI_NOT_INITED) {}

RaspiHttpNoti::~RaspiHttpNoti() {}

bool RaspiHttpNoti::IsInited() {
    return (state_ == HTTPNOTI_AVAILABLE || state_ == HTTPNOTI_IN_USE);
}

bool RaspiHttpNoti::Initialize(const std::string url) {
    if (utils::GetHardwareDeviceId(&deviceid_) == false) {
        RTC_LOG(INFO) << "Failed to get Hardware Serial from '/proc/cpuinfo'";
        return false;
    };
    RTC_LOG(INFO) << "Hardware Serial : " << deviceid_;

    // initialize with URL parameters
    rtc::Url<char> server_url(url);
    if (server_url.valid() == true) {
        server_ = server_url.host();
        port_ = server_url.port();
        path_ = server_url.path();
        RTC_LOG(INFO) << "Proxy Noti server : " << server_
                      << ", Port: " << port_ << ", Path: " << path_;
    } else {
        // URL value is invalid
        return false;
    }

    server_address_.SetIP(server_);
    server_address_.SetPort(port_);
    if (server_address_.IsUnresolvedIP()) {
        resolver_ = new rtc::AsyncResolver();
        resolver_->SignalDone.connect(this, &RaspiHttpNoti::OnResolveResult);
        resolver_->Start(server_address_);
        state_ = HTTPNOTI_ADDR_RESOLVING;
    } else {
        RTC_LOG(INFO) << "Noti Server Address : " << server_address_.ToString();
        state_ = HTTPNOTI_IFADDR_RESOLVING;
        // no need to resolve server name, so do resolve interface ip address
        DoIfAddrResolveConnect();
    }
    return true;
}

void RaspiHttpNoti::SendMotionNoti(const std::string motion_file) {
    if (IsInited() == false) {
        RTC_LOG(INFO) << "HttpNoti is not initialized. state: " << state_
                      << ",Aborting send HttpNoti message";
        return;
    }
    RTC_LOG(INFO) << " with : " << motion_file;

    char motion_file_url[2048];
    rtc::sprintfn(motion_file_url, sizeof(motion_file_url),
                  "http://%s:%i/motion/%s",
                  local_address_.ipaddr().ToString().c_str(),
                  kDefaultMotionFileServingPort, motion_file.c_str());

    char json_buffer[2048];
    rtc::sprintfn(json_buffer, sizeof(json_buffer),
                  "{\"motion_file\": \"%s\""
                  "\"motion_video_uri\" : \"%s\", \"deviceid\":\"%s\"}",
                  motion_file.c_str(), motion_file_url, deviceid_);

    char buffer[2048];
    rtc::sprintfn(buffer, sizeof(buffer),
                  "POST %s HTTP/1.0\r\n"
                  "Content-Length: %i\r\n"
                  "Connection: close\r\n"
                  "Content-Type: application/json\r\n"
                  "\r\n",
                  path_.c_str(), strlen(json_buffer));
    std::string send_data = buffer;
    send_data += json_buffer;
    // start http noti sending
    ActivateSending(send_data);
}

////////////////////////////////////////////////////////////////////////////////
//
// Find the server address and local ip address to use when sending messages.
//
////////////////////////////////////////////////////////////////////////////////

// Resolve server domain name to ip address
void RaspiHttpNoti::OnResolveResult(rtc::AsyncResolverInterface* resolver) {
    if (resolver_->GetError() != 0) {
        RTC_LOG(LS_ERROR) << "Failed to resolve server hostname : "
                          << server_address_.ToString();
        resolver_->Destroy(false);
        resolver_ = nullptr;
        state_ = HTTPNOTI_INIT_ERROR;
    } else {
        server_address_ = resolver_->address();
        RTC_LOG(INFO) << "Resolved server address : "
                      << server_address_.ToString();
        DoIfAddrResolveConnect();
    }
}

// OnConnect Callback to getting the local address from established connection
void RaspiHttpNoti::OnIfAddrConnect(rtc::AsyncSocket* socket) {
    RTC_DCHECK(socket->GetState() == rtc::AsyncSocket::CS_CONNECTED);
    RTC_DCHECK(state_ == HTTPNOTI_IFADDR_RESOLVING);
    local_address_ = socket->GetLocalAddress();
    RTC_LOG(INFO) << "Resolved local ip address : \""
                  << local_address_.ipaddr().ToString() << "\"";
    // Do close
    socket->Close();
    state_ = HTTPNOTI_AVAILABLE;
}

// making temporary TCP connenction to get local ip address
void RaspiHttpNoti::DoIfAddrResolveConnect(void) {
    state_ = HTTPNOTI_IFADDR_RESOLVING;
    http_socket_.reset(CreateClientSocket(server_address_.ipaddr().family()));

    RTC_DCHECK(http_socket_.get() != nullptr);
    // connect socket signal
    http_socket_->SignalConnectEvent.connect(this,
                                             &RaspiHttpNoti::OnIfAddrConnect);
    http_socket_->SignalCloseEvent.connect(
        this, &RaspiHttpNoti::OnIfAddrResolveClose);

    int err = http_socket_->Connect(server_address_);
    if (err == SOCKET_ERROR) {
        RTC_LOG(LS_ERROR) << "Socket error during connect : " << err;
        state_ = HTTPNOTI_INIT_ERROR;
    }
}

void RaspiHttpNoti::OnIfAddrResolveClose(rtc::AsyncSocket* socket, int err) {
    socket->Close();
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, kDelayResolveIfAddr,
                                        this, 0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Message interface is used for message transmission.
//
////////////////////////////////////////////////////////////////////////////////
void RaspiHttpNoti::ActivateSending(const std::string& noti) {
    rtc::CritScope cs(&crit_sect_);
    if (state_ == HTTPNOTI_AVAILABLE) {
        noti_message_list_.push_back(noti);
        rtc::Thread::Current()->Post(RTC_FROM_HERE, this, 0);
        state_ = HTTPNOTI_IN_USE;
    } else if (state_ == HTTPNOTI_IN_USE)
        noti_message_list_.push_back(noti);
    else
        RTC_LOG(LS_ERROR) << "State is not ready to send Noti message"
                          << ", Current State:" << state_;
}

//
void RaspiHttpNoti::DeactivateSending(void) {
    RTC_DCHECK(noti_message_list_.size() == 0);
    rtc::CritScope cs(&crit_sect_);
    if (state_ == HTTPNOTI_IN_USE) {
        state_ = HTTPNOTI_AVAILABLE;
    } else if (state_ == HTTPNOTI_AVAILABLE)
        RTC_LOG(LS_ERROR) << "Already Noti Available State";
    else
        RTC_LOG(LS_ERROR) << "State is not ready to send Noti message"
                          << ", Current State:" << state_;
}

void RaspiHttpNoti::OnMessage(rtc::Message* msg) {
    if (state_ == HTTPNOTI_IFADDR_RESOLVING) {
        RTC_LOG(INFO) << "Do resolving IP address of network interface";
        DoIfAddrResolveConnect();
    } else {
        RTC_LOG(INFO) << "Trying to connect Noti Proxy.";
        DoConnect();
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// sends Noti Message after Http connection.
//
////////////////////////////////////////////////////////////////////////////////
void RaspiHttpNoti::DoConnect(void) {
    http_socket_.reset(CreateClientSocket(server_address_.ipaddr().family()));
    state_ = HTTPNOTI_IN_USE;

    RTC_DCHECK(http_socket_.get() != nullptr);
    // connect socket signal
    http_socket_->SignalCloseEvent.connect(this, &RaspiHttpNoti::OnClose);
    http_socket_->SignalConnectEvent.connect(this, &RaspiHttpNoti::OnConnect);
    http_socket_->SignalReadEvent.connect(this, &RaspiHttpNoti::OnRead);

    int err = http_socket_->Connect(server_address_);
    if (err == SOCKET_ERROR) {
        RTC_LOG(LS_ERROR) << "Socket error during connect : " << err;
        Close();
    }
}

void RaspiHttpNoti::Close() {
    http_socket_->Close();
    response_data_.clear();
    if (resolver_ != nullptr) {
        resolver_->Destroy(false);
        resolver_ = nullptr;
    }
    state_ = HTTPNOTI_AVAILABLE;
}

void RaspiHttpNoti::OnConnect(rtc::AsyncSocket* socket) {
    std::string noti_message = noti_message_list_.front();
    if (noti_message.length() == 0) {
        RTC_LOG(LS_ERROR)
            << "HttpNoti Nothing to send, message queue is empty.";
        Close();  // close http socket
        return;
    }
    size_t sent = socket->Send(noti_message.c_str(), noti_message.length());
    if (sent != noti_message.length()) {
        RTC_LOG(LS_ERROR) << "Failed to send data. mesg size: "
                          << noti_message.length() << ", sent : " << sent;
    }
    RTC_LOG(INFO) << "HttpNoti Sent success: " << noti_message;
    noti_message_list_.pop_front();
}

void RaspiHttpNoti::OnClose(rtc::AsyncSocket* socket, int err) {
    if (err) RTC_LOG(INFO) << "HttpNoti OnClose Error code: " << err;
    socket->Close();
    if (noti_message_list_.size() > 0) {
        if (err == ECONNREFUSED) {
            // connection refused, trying to send it again
            rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE,
                                                kDelayResendActivate, this, 0);
        } else {
            // send successful/another network error
            rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE,
                                                kDelaySendActivate, this, 0);
        }
    } else {
        DeactivateSending();
    }
}

bool RaspiHttpNoti::GetHeaderValue(const std::string& data, size_t eoh,
                                   const char* header_pattern, size_t* value) {
    RTC_DCHECK(value != nullptr);
    size_t found = data.find(header_pattern);
    if (found != std::string::npos && found < eoh) {
        *value = atoi(&data[found + strlen(header_pattern)]);
        return true;
    }
    return false;
}

bool RaspiHttpNoti::GetHeaderValue(const std::string& data, size_t eoh,
                                   const char* header_pattern,
                                   std::string* value) {
    RTC_DCHECK(value != NULL);
    size_t found = data.find(header_pattern);
    if (found != std::string::npos && found < eoh) {
        size_t begin = found + strlen(header_pattern);
        size_t end = data.find("\r\n", begin);
        if (end == std::string::npos) end = eoh;
        value->assign(data.substr(begin, end - begin));
        return true;
    }
    return false;
}

bool RaspiHttpNoti::ReadIntoBuffer(rtc::AsyncSocket* socket, std::string& data,
                                   size_t* content_length) {
    RTC_DCHECK(state_ == HTTPNOTI_IN_USE);
    char buffer[kMaxBufferSize];
    do {
        int bytes = socket->Recv(buffer, sizeof(buffer), nullptr);
        if (bytes <= 0) break;
        data.append(buffer, bytes);
    } while (true);

    bool ret = false;
    size_t i = data.find("\r\n\r\n");
    if (i != std::string::npos) {
        RTC_LOG(INFO) << "Headers received";
        if (GetHeaderValue(data, i, "\r\nContent-Length: ", content_length)) {
            size_t total_response_size = (i + 4) + *content_length;
            if (data.length() >= total_response_size) {
                ret = true;
                std::string should_close;
                const char kConnection[] = "\r\nConnection: ";
                if (GetHeaderValue(data, i, kConnection, &should_close) &&
                    should_close.compare("close") == 0) {
                    socket->Close();
                    // Since we closed the socket, there was no notification
                    // delivered to us.  Compensate by letting ourselves know.
                    OnClose(socket, 0);
                }
            } else {
                // We haven't received everything.  Just continue to accept
                // data.
            }
        } else {
            RTC_LOG(LS_ERROR)
                << "No content length field specified by the server.";
            RTC_LOG(INFO) << "Readed Response : " << data;
            OnClose(socket, 0);
        }
    }
    return ret;
}

int RaspiHttpNoti::GetResponseStatus(const std::string& response) {
    int status = -1;
    size_t pos = response.find(' ');
    if (pos != std::string::npos) status = atoi(&response[pos + 1]);
    return status;
}

void RaspiHttpNoti::OnRead(rtc::AsyncSocket* socket) {
    size_t content_length = 0;
    int status;
    if (ReadIntoBuffer(socket, response_data_, &content_length)) {
        status = GetResponseStatus(response_data_);
        if (status == 200) {
            RTC_LOG(INFO) << "HttpNoti Got 200 OK Response";
        } else {
            RTC_LOG(LS_ERROR) << "HttpNoti HTTP ERROR : " << status;
        }
    }
    socket->Close();
}
