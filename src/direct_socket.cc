/*
Copyright (c) 2016, rpi-webrtc-streamer Lyu,KeunChang

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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(WEBRTC_POSIX)
#include <unistd.h>
#endif
#include "direct_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/time_utils.h"
#include "streamer_observer.h"
#include "utils.h"

// delay of message to use for stream release
static const int kStreamReleaseDelay = 1000;
static const std::string kDirectSocketDelimiter = "\n";

////////////////////////////////////////////////////////////////////////////////
// DirectSocketServer
////////////////////////////////////////////////////////////////////////////////

DirectSocketServer::DirectSocketServer()
    : listener_(nullptr), direct_socket_(nullptr) {
    last_reject_time_ms_ = connection_reject_count_ = 0;
}
DirectSocketServer::~DirectSocketServer() {}

bool DirectSocketServer::Listen(const rtc::SocketAddress& address) {
    rtc::Thread* thread = rtc::Thread::Current();
    RTC_DCHECK(thread != nullptr);
    rtc::AsyncSocket* sock = thread->socketserver()->CreateAsyncSocket(
        address.family(), SOCK_STREAM);
    if (!sock) {
        return false;
    }
    listener_.reset(sock);
    listener_->SignalReadEvent.connect(this, &DirectSocketServer::OnAccept);
    if ((listener_->Bind(address) != SOCKET_ERROR) &&
        (listener_->Listen(5) != SOCKET_ERROR)) {
        RTC_LOG(INFO) << "Start listening " << address.ToString() << ".";
        return true;
    }
    RTC_LOG(LS_ERROR) << "Failed to listen " << address.ToString() << ".";
    return false;
}

void DirectSocketServer::StopListening(void) {
    if (listener_) {
        listener_->Close();
    }
}

void DirectSocketServer::OnAccept(rtc::AsyncSocket* socket) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(socket == listener_.get());
    rtc::AsyncSocket* incoming = listener_->Accept(nullptr);
    RTC_CHECK(incoming != nullptr);

    if (IsStreamSessionActive() == false) {
        // stream session is not active, it will start to new stream session
        connection_reject_count_ = 0;  // reinitialize the reject counter

        socket_peer_id_ = DIRECTSOCKET_FAKE_PEERID;
        socket_peer_name_ = DIRECTSOCKET_FAKE_NAME_PREFIX +
                            incoming->GetRemoteAddress().ToString();
        RTC_LOG(INFO) << "New Session Name: " << socket_peer_name_;

        if (ActivateStreamSession(socket_peer_id_, socket_peer_name_) == true) {
            direct_socket_.reset(incoming);
            // Close event will be handled in DirectSocketServer
            incoming->SignalCloseEvent.connect(this,
                                               &DirectSocketServer::OnClose);
            incoming->SignalReadEvent.connect(this,
                                              &DirectSocketServer::OnRead);
        } else {
            // reject the direct socket request from client
            incoming->Close();
        }
    } else {
        // reject the direct socket request from client
        incoming->Close();

        // reset or re-calculate reject counter
        if (rtc::TimeDiff(rtc::TimeMillis(), last_reject_time_ms_) <
            FORCE_CONNECTION_DROP_VALID_DURATION) {
            connection_reject_count_ += 1;
            last_reject_time_ms_ = rtc::TimeMillis();
            RTC_LOG(INFO) << "Forced Connection Counter: "
                          << connection_reject_count_;
            if (connection_reject_count_ >
                FORCE_CONNECTION_DROP_TRYCOUNT_THRESHOLD)
                RTC_LOG(INFO) << "Forced Connection close performed, "
                              << " counter reached to threahold.";
            // Release the current active stream session
            OnClose(direct_socket_.get(), 0);
        } else {
            connection_reject_count_ = 0;
            last_reject_time_ms_ = rtc::TimeMillis();
        }
    }
}

//
void DirectSocketServer::RegisterObserver(StreamerObserver* callback) {
    RTC_LOG(INFO) << __FUNCTION__;
}

void DirectSocketServer::OnClose(rtc::AsyncSocket* socket, int err) {
    RTC_LOG(INFO) << __FUNCTION__ << ", Error: " << err;

    // Close the stream session
    socket->SignalCloseEvent.disconnect(this);
    socket->Close();

    DeactivateStreamSession();
}

void DirectSocketServer::OnRead(rtc::AsyncSocket* socket) {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(socket != nullptr);

    char buffer[4096] = {0x00};
    int bytes = 0;

    do {
        bytes = socket->Recv(buffer, sizeof(buffer), nullptr);
        if (bytes <= 0) break;
        buffered_read_.append(buffer, bytes);
    } while (true);

    std::string string_buffer;
    std::string line_message;
    size_t pos;

    string_buffer = buffered_read_;
    while ((pos = string_buffer.find(kDirectSocketDelimiter)) !=
           std::string::npos) {
        line_message = string_buffer.substr(0, pos);
        RTC_LOG(INFO) << "Extracted Line: " << line_message;
        string_buffer.erase(0, pos + kDirectSocketDelimiter.length());
        MessageFromPeer(line_message);
    }
    // keep the left string in original buffered read string
    buffered_read_ = string_buffer;
    if (buffered_read_.length()) {
        // The following message is not a log that notifies you of an error
        // but tells you when there is an error and a log message for debugging.
        // If the socket re-reads the tcp mesage with a delimiter,
        // the read message will be appended to the end and processed.
        RTC_LOG(LS_WARNING) << "Direct Buffer Left : " << buffered_read_;
    }
}

bool DirectSocketServer::SendMessageToPeer(const int peer_id,
                                           const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__;
    std::string trimmed_message(message);
    trimmed_message.erase(
        std::remove(trimmed_message.begin(), trimmed_message.end(), '\n'),
        trimmed_message.end());
    trimmed_message.append("\n");

    RTC_LOG(INFO) << "Message OUT to client: \"" << trimmed_message << "\"";
    return (direct_socket_->Send(trimmed_message.data(),
                                 trimmed_message.length()) != SOCKET_ERROR);
}

void DirectSocketServer::ReportEvent(const int peer_id, bool drop_connection,
                                     const std::string& message) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (drop_connection == true) {
        rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, kStreamReleaseDelay,
                                            this, 3400);
    }
    return;
}

void DirectSocketServer::OnMessage(rtc::Message* msg) {
    RTC_DCHECK(msg->message_id == 3400);

    //  Internally, there is no reason to keep the session anymore,
    //  so it terminates the session that is currently being held.
    RTC_LOG(INFO) << __FUNCTION__ << "Drop Direct Socket Connection.";
    if (IsStreamSessionActive() == true) {
        // Release the current active stream session
        OnClose(direct_socket_.get(), 0);
        DeactivateStreamSession();
    };
    return;
}
