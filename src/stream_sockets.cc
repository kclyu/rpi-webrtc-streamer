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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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


////////////////////////////////////////////////////////////////////////////////
// StreamSocketListen
////////////////////////////////////////////////////////////////////////////////

StreamSocketListen::StreamSocketListen()
    : listener_(nullptr) {
}
StreamSocketListen::~StreamSocketListen() {}

bool StreamSocketListen::Listen(const rtc::SocketAddress& address) {
    LOG(INFO) << __FUNCTION__;
    rtc::Thread *thread = rtc::Thread::Current();
    RTC_DCHECK(thread != NULL);
    rtc::AsyncSocket* sock =
        thread->socketserver()->CreateAsyncSocket(address.family(), SOCK_STREAM);
    if (!sock) {
        return false;
    }
    listener_.reset(sock);
    listener_->SignalReadEvent.connect(this, &StreamSocketListen::OnAccept);
    if ((listener_->Bind(address) != SOCKET_ERROR) &&
            (listener_->Listen(5) != SOCKET_ERROR)) {
        LOG(INFO) << "Start listening " << address  << ".";
        return true;
    }
    LOG(LS_ERROR) << "Failed to listen " << address  << ".";
    return false;
}


void StreamSocketListen::StopListening(void) {
    if (listener_) {
        listener_->Close();
    }
}

void StreamSocketListen::OnAccept(rtc::AsyncSocket* socket) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(socket == listener_.get());
    rtc::AsyncSocket* incoming = listener_->Accept(NULL);
    RTC_DCHECK(incoming != NULL );
    // Close event will be handled in StreamSocketListen
    incoming->SignalCloseEvent.connect(this, &StreamSocketListen::OnClose);
    stream_sockets_.CreateStreamConnection(incoming);
    // stream_sockets_.DumpStreamConnectionsAll();
}

void StreamSocketListen::OnClose(rtc::AsyncSocket* socket, int err) {
    LOG(INFO) << __FUNCTION__ << ", Error: " << err ;
    socket->SignalCloseEvent.disconnect(this);
    stream_sockets_.DestroyStreamConnection(socket);
    socket->Close();
}

////////////////////////////////////////////////////////////////////////////////
// StreamSockets
////////////////////////////////////////////////////////////////////////////////
StreamSockets::StreamSockets()  {
}
StreamSockets::~StreamSockets() {
    for (std::vector<StreamConnection *>::const_iterator it = stream_connections_.begin();
            it != stream_connections_.end(); ++it) {
        delete *it;
    }
    stream_connections_.clear();
}

bool StreamSockets::CreateStreamConnection(rtc::AsyncSocket* socket) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK( socket != NULL );
    StreamConnection *sc = new StreamConnection(socket);
    stream_connections_.push_back(sc);
    socket->SignalReadEvent.connect(this, &StreamSockets::OnRead);
    return true;
}

bool StreamSockets::DestroyStreamConnection(rtc::AsyncSocket* socket) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK( socket != NULL );
    socket->SignalReadEvent.disconnect(this);
    StreamConnection *sc =  Lookup(socket);
    stream_connections_.erase(std::remove(stream_connections_.begin(), stream_connections_.end(), sc),
                              stream_connections_.end());
    return false;
}

void StreamSockets::DumpStreamConnections(StreamConnection *sc) {
    sc->DumpStreamConnection();
}


void StreamSockets::DumpStreamConnectionsAll(void) {
    LOG(INFO) << __FUNCTION__;
    LOG(INFO) << "StreamConnection count: " << stream_connections_.size();
    for (std::vector<StreamConnection *>::const_iterator it = stream_connections_.begin();
            it != stream_connections_.end(); ++it) {
        DumpStreamConnections( *it );
    }
}

StreamConnection *StreamSockets::Lookup(rtc::AsyncSocket* socket) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK( socket != NULL );
    for (std::vector<StreamConnection *>::const_iterator it = stream_connections_.begin();
            it != stream_connections_.end(); ++it) {
        if( (*it)->Match(socket) ) return *it;
    }
    return nullptr;
}


void StreamSockets::OnRead(rtc::AsyncSocket* socket) {
    LOG(INFO) << __FUNCTION__;
    RTC_DCHECK( socket != NULL );
    StreamConnection *sc =  Lookup(socket);
    RTC_DCHECK( sc->Match(socket));
    RTC_DCHECK( sc->Valid() );

    char buffer[4096]= {0x00};
    int	bytes = 0;
    bytes = socket->Recv(buffer, sizeof(buffer), nullptr);
    // mark additional null termiantion
    buffer[bytes+1] = 0x00;

    if( bytes > 0 ) {
        if( sc->HandleReceivedData(buffer, bytes) ) {
            DumpStreamConnectionsAll();
            sc->HandleRequest();
        }
    }
}


