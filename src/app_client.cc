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
#include "app_client.h"

static const uint32_t kClientIDLength = 8;
static const uint32_t kRoomIDLength = 9;
static const uint32_t kMaxClientID     = 99999999;
static const uint32_t kMaxRoomID = 999999999;

//////////////////////////////////////////////////////////////////////////////////////////
//
// AppClientInfo
//
//////////////////////////////////////////////////////////////////////////////////////////
AppClientInfo::AppClientInfo () : 
        status_(ClientStatus::CLIENT_NOTCONNECTED) {
}


ClientStatus AppClientInfo::Status (int client_id)  {
    RTC_DCHECK( client_id_ == client_id );
    return status_;
}

bool AppClientInfo::GetWebSocketId (int client_id, int &websocket_id)  {
    if( client_id_ == client_id ) return false;
    if( status_ ==  ClientStatus::CLIENT_CONNECTED )  {
        websocket_id = websocket_id_;
        return true;
    };
    return false;
}

bool AppClientInfo::Connected (int room_id)  {
    int client_id;
    int min, max;

    min = kMaxClientID / 10;
    max = kMaxClientID;
    client_id = min + (rtc::CreateRandomId() % (int)(max - min + 1));
    return Connected( room_id, client_id );
}

bool AppClientInfo::Connected (int room_id, int client_id)  {
    switch( status_ ) {
        case ClientStatus::CLIENT_CONNECTED:
            RTC_DCHECK( room_id_ == room_id && client_id_ == client_id );
            return true;
        case ClientStatus::CLIENT_UNKNOWN:
        case ClientStatus::CLIENT_CLOSING:
            return false;
        case ClientStatus::CLIENT_ERROR:
        case ClientStatus::CLIENT_NOTCONNECTED:
            status_ = ClientStatus::CLIENT_CONNECTED;
            room_id_ = room_id;
            client_id_ = client_id;
            return true;
    }
    return false;
}

bool AppClientInfo::Connected (int websocket_id, int room_id, int client_id)  {
    websocket_id_ = websocket_id;
    return Connected(room_id, client_id);
}

bool AppClientInfo::Closing (int client_id)  {
    RTC_DCHECK( client_id_ == client_id );
    switch( status_ ) {
        case ClientStatus::CLIENT_CONNECTED:
        case ClientStatus::CLIENT_CLOSING:
            status_ = ClientStatus::CLIENT_CLOSING;
            return true;
        default:
            break;
    }
    return false;
}

bool AppClientInfo::Closed (int client_id)  {
    RTC_DCHECK( client_id_ == client_id );
    switch( status_ ) {
        case ClientStatus::CLIENT_CONNECTED:
        case ClientStatus::CLIENT_CLOSING:
            Reset();
            return true;
        default:
            break;
    }
    return false;
}

void AppClientInfo::Reset ()  {
    status_ = ClientStatus::CLIENT_NOTCONNECTED;
    client_id_ = room_id_ = websocket_id_ = 0; 
}

