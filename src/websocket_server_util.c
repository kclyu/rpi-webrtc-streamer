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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "websocket_server_internal.h"

void dump_handshake_info(struct lws *wsi) {
    int n = 0, len;
    char buf[256];
    const unsigned char *c;

    do {
        c = lws_token_to_string(n);
        if (!c) {
            n++;
            continue;
        }

        len = lws_hdr_total_length(wsi, n);
        if (!len || len > sizeof(buf) - 1) {
            n++;
            continue;
        }

        lws_hdr_copy(wsi, buf, sizeof buf, n);
        buf[sizeof(buf) - 1] = '\0';

        lwsl_debug("    %s(%d) = %s\n", (char *)c, n, buf);
        n++;
    } while (c);
}

const char *to_callbackreason_str(int callback_reason, int brief) {
    switch (callback_reason) {
        case LWS_CALLBACK_ESTABLISHED:
            return "LWS_CALLBACK_ESTABLISHED";
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            return "LWS_CALLBACK_CLIENT_CONNECTION_ERROR";
        case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
            return "LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH";
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            return "LWS_CALLBACK_CLIENT_ESTABLISHED";
        case LWS_CALLBACK_CLOSED:
            return "LWS_CALLBACK_CLOSED";
        case LWS_CALLBACK_CLOSED_HTTP:
            return "LWS_CALLBACK_CLOSED_HTTP";
        case LWS_CALLBACK_RECEIVE:
            return "LWS_CALLBACK_RECEIVE";
        case LWS_CALLBACK_RECEIVE_PONG:
            return "LWS_CALLBACK_RECEIVE_PONG";
        case LWS_CALLBACK_CLIENT_RECEIVE:
            return "LWS_CALLBACK_CLIENT_RECEIVE";
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            return "LWS_CALLBACK_CLIENT_RECEIVE_PONG";
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            return "LWS_CALLBACK_CLIENT_WRITEABLE";
        case LWS_CALLBACK_SERVER_WRITEABLE:
            return "LWS_CALLBACK_SERVER_WRITEABLE";
        case LWS_CALLBACK_HTTP:
            return "LWS_CALLBACK_HTTP";
        case LWS_CALLBACK_HTTP_BODY:
            return "LWS_CALLBACK_HTTP_BODY";
        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
            return "LWS_CALLBACK_HTTP_BODY_COMPLETION";
        case LWS_CALLBACK_HTTP_FILE_COMPLETION:
            return "LWS_CALLBACK_HTTP_FILE_COMPLETION";
        case LWS_CALLBACK_HTTP_WRITEABLE:
            return "LWS_CALLBACK_HTTP_WRITEABLE";
        case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
            return "LWS_CALLBACK_FILTER_NETWORK_CONNECTION";
        case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
            return "LWS_CALLBACK_FILTER_HTTP_CONNECTION";
        case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
            return "LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED";
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
            return "LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION";
        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
            return "LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS";
        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
            return "LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS";
        case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION:
            return "LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION";
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
            return "LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER";
        case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
            return "LWS_CALLBACK_CONFIRM_EXTENSION_OKAY";
        case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
            return "LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED";
        case LWS_CALLBACK_PROTOCOL_INIT:
            return "LWS_CALLBACK_PROTOCOL_INIT";
        case LWS_CALLBACK_PROTOCOL_DESTROY:
            return "LWS_CALLBACK_PROTOCOL_DESTROY";
        case LWS_CALLBACK_WSI_CREATE:
            return "LWS_CALLBACK_WSI_CREATE";
        case LWS_CALLBACK_WSI_DESTROY:
            return "LWS_CALLBACK_WSI_DESTROY";
        case LWS_CALLBACK_GET_THREAD_ID:
            return NULL;
        case LWS_CALLBACK_ADD_POLL_FD:
            return "LWS_CALLBACK_ADD_POLL_FD";
        case LWS_CALLBACK_DEL_POLL_FD:
            return "LWS_CALLBACK_DEL_POLL_FD";
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
            if (brief) return NULL;
            return "LWS_CALLBACK_CHANGE_MODE_POLL_FD";
        case LWS_CALLBACK_LOCK_POLL:
            if (brief) return NULL;
            return "LWS_CALLBACK_LOCK_POLL";
        case LWS_CALLBACK_UNLOCK_POLL:
            if (brief) return NULL;
            return "LWS_CALLBACK_UNLOCK_POLL";
        case LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY:
            return "LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY";
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
            return "LWS_CALLBACK_WS_PEER_INITIATED_CLOSE";
        case LWS_CALLBACK_WS_EXT_DEFAULTS:
            return "LWS_CALLBACK_WS_EXT_DEFAULTS";
        case LWS_CALLBACK_CGI:
            return "LWS_CALLBACK_CGI";
        case LWS_CALLBACK_CGI_TERMINATED:
            return "LWS_CALLBACK_CGI_TERMINATED";
        case LWS_CALLBACK_CGI_STDIN_DATA:
            return "LWS_CALLBACK_CGI_STDIN_DATA";
        case LWS_CALLBACK_CGI_STDIN_COMPLETED:
            return "LWS_CALLBACK_CGI_STDIN_COMPLETED";
        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
            return "LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP";
        case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
            return "LWS_CALLBACK_CLOSED_CLIENT_HTTP";
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
            return "LWS_CALLBACK_RECEIVE_CLIENT_HTTP";
        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
            return "LWS_CALLBACK_COMPLETED_CLIENT_HTTP";
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
            return "LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ";
        case LWS_CALLBACK_HTTP_BIND_PROTOCOL:
            return "LWS_CALLBACK_HTTP_BIND_PROTOCOL";
        case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
            return "LWS_CALLBACK_HTTP_DROP_PROTOCOL";
        default:
            return "Callback reason does not defined in string reason.";
    }
}

const char *to_httpstatus_str(int status) {
    switch (status) {
        case HTTP_STATUS_OK:
            return "HTTP_STATUS_OK";
        case HTTP_STATUS_NO_CONTENT:
            return "HTTP_STATUS_NO_CONTENT";
        case HTTP_STATUS_PARTIAL_CONTENT:
            return "HTTP_STATUS_PARTIAL_CONTENT";
        case HTTP_STATUS_MOVED_PERMANENTLY:
            return "HTTP_STATUS_MOVED_PERMANENTLY";
        case HTTP_STATUS_FOUND:
            return "HTTP_STATUS_FOUND";
        case HTTP_STATUS_SEE_OTHER:
            return "HTTP_STATUS_SEE_OTHER";
        case HTTP_STATUS_UNAUTHORIZED:
            return "HTTP_STATUS_UNAUTHORIZED";
        case HTTP_STATUS_PAYMENT_REQUIRED:
            return "HTTP_STATUS_PAYMENT_REQUIRED";
        case HTTP_STATUS_FORBIDDEN:
            return "HTTP_STATUS_FORBIDDEN";
        case HTTP_STATUS_NOT_FOUND:
            return "HTTP_STATUS_NOT_FOUND";
        case HTTP_STATUS_METHOD_NOT_ALLOWED:
            return "HTTP_STATUS_METHOD_NOT_ALLOWED";
        case HTTP_STATUS_NOT_ACCEPTABLE:
            return "HTTP_STATUS_NOT_ACCEPTABLE";
        case HTTP_STATUS_PROXY_AUTH_REQUIRED:
            return "HTTP_STATUS_PROXY_AUTH_REQUIRED";
        case HTTP_STATUS_REQUEST_TIMEOUT:
            return "HTTP_STATUS_REQUEST_TIMEOUT";
        case HTTP_STATUS_CONFLICT:
            return "HTTP_STATUS_CONFLICT";
        case HTTP_STATUS_GONE:
            return "HTTP_STATUS_GONE";
        case HTTP_STATUS_LENGTH_REQUIRED:
            return "HTTP_STATUS_LENGTH_REQUIRED";
        case HTTP_STATUS_PRECONDITION_FAILED:
            return "HTTP_STATUS_PRECONDITION_FAILED";
        case HTTP_STATUS_REQ_ENTITY_TOO_LARGE:
            return "HTTP_STATUS_REQ_ENTITY_TOO_LARGE";
        case HTTP_STATUS_REQ_URI_TOO_LONG:
            return "HTTP_STATUS_REQ_URI_TOO_LONG";
        case HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:
            return "HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE";
        case HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE:
            return "HTTP_STATUS_REQ_RANGE_NOT_SATISFIABLE";
        case HTTP_STATUS_EXPECTATION_FAILED:
            return "HTTP_STATUS_EXPECTATION_FAILED";
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
            return "HTTP_STATUS_INTERNAL_SERVER_ERROR";
        case HTTP_STATUS_NOT_IMPLEMENTED:
            return "HTTP_STATUS_NOT_IMPLEMENTED";
        case HTTP_STATUS_BAD_GATEWAY:
            return "HTTP_STATUS_BAD_GATEWAY";
        case HTTP_STATUS_SERVICE_UNAVAILABLE:
            return "HTTP_STATUS_SERVICE_UNAVAILABLE";
        case HTTP_STATUS_GATEWAY_TIMEOUT:
            return "HTTP_STATUS_GATEWAY_TIMEOUT";
        case HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED:
            return "HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED";
        default:
            return "HTTP_UNKNOWN_STATUS";
    };
}
