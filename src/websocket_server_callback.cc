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

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "utils.h"
#include "websocket_server.h"

static size_t LWS_PRE_SIZE = LWS_PRE;

// ring buffer message
struct msg {
    void *payload; /* is malloc'd */
    size_t len;
    char binary;
    char first;
    char final;
};

//////////////////////////////////////////////////////////////////////
//
// Macros used in this callback processing
//
//////////////////////////////////////////////////////////////////////

#define INTERNAL__GET_WSSINSTANCE \
    reinterpret_cast<LibWebSocketServer *>(lws_context_user(vhd->context))

#define INTERNAL__GET_HTTPHANDLER                                           \
    (reinterpret_cast<LibWebSocketServer *>(lws_context_user(vhd->context)) \
         ->GetHttpHandler(pss->uri_path_))

#define INTERNAL__GET_WEBSOCKETHANDLER                                      \
    if ((handler = reinterpret_cast<LibWebSocketServer *>(                  \
                       lws_context_user(vhd->context))                      \
                       ->GetWebsocketHandler(pss->uri_path_)) == nullptr) { \
        RTC_LOG(LS_ERROR) << "Callback Handler is not found in URI: "       \
                          << pss->uri_path_;                                \
        return 0;                                                           \
    };

//////////////////////////////////////////////////////////////////////
//
// Main WebSocket callback for handling file,http,websocket
//
//////////////////////////////////////////////////////////////////////
int LibWebSocketServer::CallbackLibWebsockets(struct lws *wsi,
                                              enum lws_callback_reasons reason,
                                              void *user, void *in,
                                              size_t len) {
    struct per_session_data__libwebsockets *pss =
        (struct per_session_data__libwebsockets *)user;
    struct vhd_libwebsocket *vhd =
        (struct vhd_libwebsocket *)lws_protocol_vh_priv_get(
            lws_get_vhost(wsi), lws_get_protocol(wsi));
    const char *reason_str;
    int sockid;

    sockid = lws_get_socket_fd(wsi);
    if ((reason_str = to_callbackreason_str(reason, 1)))
        lwsl_debug("**** %s(%d):: sock(%d),len(%d)\n", reason_str, reason,
                   sockid, (int)len);

    switch (reason) {
            //
            // initialize private vhd
            //
        case LWS_CALLBACK_PROTOCOL_INIT:
            vhd = reinterpret_cast<struct vhd_libwebsocket *>(
                lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                                            lws_get_protocol(wsi),
                                            sizeof(struct vhd_libwebsocket)));
            if (!vhd) return -1;

            vhd->context = lws_get_context(wsi);
            vhd->vhost = lws_get_vhost(wsi);
            break;

        case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
            return 1; /* disallow compression */

        case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
            lwsl_info("LWS_CALLBACK_EVENT_WAIT_CANCELLED\n");
            break;

            //
            // WebSocket Callback Processing
            //
        case LWS_CALLBACK_ESTABLISHED: {
            WSInternalHandlerConfig *handler;

            INTERNAL__GET_WEBSOCKETHANDLER;
            handler->CreateHandlerRuntime(sockid, wsi);
            handler->OnConnect(sockid);
        } break;

        case LWS_CALLBACK_CLOSED:
            RTC_DCHECK(sockid > 0);
            {
                WSInternalHandlerConfig *handler;
                INTERNAL__GET_WEBSOCKETHANDLER
                handler->OnDisconnect(sockid);
            }
            break;

        case LWS_CALLBACK_RECEIVE:
            lwsl_debug(
                "LWS_CALLBACK_RECEIVE: %4d (rpp %5d, first %d, "
                "last %d, bin %d, len: %d))\n",
                (int)len, (int)lws_remaining_packet_payload(wsi),
                lws_is_first_fragment(wsi), lws_is_final_fragment(wsi),
                lws_frame_is_binary(wsi), (int)len);

            if (len <= 0) {
                RTC_LOG(INFO) << __FUNCTION__ << "LWS_CALLBACK_RECEIVE"
                              << "Length is below zero ****";
                break;
            }
            RTC_LOG(INFO) << __FUNCTION__ << "LWS_CALLBACK_RECEIVE";
            {
                WSInternalHandlerConfig *handler;
                INTERNAL__GET_WEBSOCKETHANDLER
                if (handler->OnMessage(
                        sockid, std::string((const char *)in, len)) == false) {
                    lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, nullptr, 0);
                    // disconnect current session
                    return -1;
                }
            }
            return 0;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            RTC_DCHECK(sockid > 0);
            {
                WSInternalHandlerConfig *handler;
                std::string message_to_peer;
                int num_sent, message_length;
                char *payload = nullptr;

                INTERNAL__GET_WEBSOCKETHANDLER

                if (handler->DequeueMessage(sockid, message_to_peer)) {
                    message_length = message_to_peer.length();
                    payload = (char *)malloc(LWS_PRE_SIZE + message_length);
                    memcpy(payload + LWS_PRE_SIZE, message_to_peer.c_str(),
                           message_length);

                    num_sent =
                        lws_write(wsi, (unsigned char *)payload + LWS_PRE_SIZE,
                                  message_length, LWS_WRITE_TEXT);
                    if (num_sent < 0) {
                        RTC_LOG(LS_ERROR)
                            << "ERROR " << num_sent
                            << " writing to socket for sending meesage to peer";
                        free(payload);
                        return -1;
                    }
                    if (num_sent < message_length)
                        RTC_LOG(LS_ERROR)
                            << "ERROR socket partial write " << num_sent
                            << " vs " << message_length;
                    free(payload);
                }
            }
            return 0;

        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION: {
            static const char *kReason_UriTooLong = "URI is too long";
            static const char *kReason_UriNotExist = "URI does not exist";
            dump_handshake_info(wsi);

            if (user)
                memset(user, 0x00, sizeof(per_session_data__libwebsockets));
            int len = lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI);
            if (!len || (size_t)len > sizeof(pss->uri_path_) - 1) {
                // reject this connection when the URI exceed the uri path
                // buffer size
                lws_close_reason(wsi, LWS_CLOSE_STATUS_ABNORMAL_CLOSE,
                                 (unsigned char *)kReason_UriTooLong,
                                 strlen(kReason_UriTooLong));
                return -1;
            }
            lws_hdr_copy(wsi, pss->uri_path_, sizeof(pss->uri_path_),
                         WSI_TOKEN_GET_URI);
            // check whether requested uri is in the he handler config
            if (INTERNAL__GET_WSSINSTANCE->IsValidWSPath(pss->uri_path_))
                RTC_LOG(INFO) << "URI Path exist " << pss->uri_path_;
            else {
                RTC_LOG(LS_ERROR)
                    << "URI Path does not exist " << pss->uri_path_
                    << ", droping this connection\n";
                // reject the connection
                // lws_rx_flow_control(wsi, 0);
                lws_close_reason(wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION,
                                 (unsigned char *)kReason_UriNotExist,
                                 strlen(kReason_UriNotExist));
                return -1;
            }
        }
            return 0;

        default:
            break;
    }
    // return 0;
    return lws_callback_http_dummy(wsi, reason, user, in, len);
}
