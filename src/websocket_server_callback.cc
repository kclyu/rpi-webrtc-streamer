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

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#include "websocket_server.h"
#include "../lib/private-libwebsockets.h"
#include "utils.h"

static size_t LWS_PRE_SIZE = LWS_PRE;
#define LIBWEBSOCKET_BUFFER_SIZE 4096

//////////////////////////////////////////////////////////////////////
//
// Macros used in this callback processing
//
//////////////////////////////////////////////////////////////////////
#define INTERNAL__GET_WSSINSTANCE  \
    reinterpret_cast<LibWebSocketServer *>(wsi->protocol->user)

#define INTERNAL__GET_HTTPHANDLER  reinterpret_cast<LibWebSocketServer *> \
        (wsi->protocol->user)->GetHttpHandler(pss->uri_path)

#define INTERNAL__GET_WEBSOCKETHANDLER \
    if( (handler = reinterpret_cast<LibWebSocketServer *> \
            (wsi->protocol->user)->GetWebsocketHandler(pss->uri_path)) \
            == nullptr ) {  \
        RTC_LOG(LS_ERROR) << "Callback Handler is not found in URI: "  \
            << pss->uri_path; \
        return 0;\
    };


//////////////////////////////////////////////////////////////////////
//
// Helper functions for build HttpRequest Type
//
//////////////////////////////////////////////////////////////////////
static void BuildHttpRequestFromWsi(struct lws *wsi, char *url_path,
        HttpRequest* http_request) {
    RTC_DCHECK(http_request != nullptr );
    unsigned int n = 0, len;
    char buf[512];
    const unsigned char *c;

    http_request->url_ = url_path;
    // Adding headers
    do {
        c = lws_token_to_string((lws_token_indexes)n);
        if (!c) {
            n++;
            continue;
        }

        len = lws_hdr_total_length(wsi, (lws_token_indexes)n);
        if (!len || len > sizeof(buf) - 1) {
            n++;
            continue;
        }

        lws_hdr_copy(wsi, buf, sizeof(buf), (lws_token_indexes)n);
        buf[sizeof(buf) - 1] = '\0';

        if ( n ==  WSI_TOKEN_HTTP_URI_ARGS )
            // add argument parameter
            http_request->AddArgs(buf);
        else if ( n ==  WSI_TOKEN_HTTP_CONTENT_LENGTH ) {
            // Getting contents length
            int int_value;
            if( utils::StringToInt( buf, &int_value ) )
                http_request->content_length_ = int_value;
        }
        else if ( n ==  WSI_TOKEN_HOST ) {
            // Getting server name
            http_request->server_ = buf;
        }
        else
            http_request->AddHeader( n, buf );
        n++;
    } while (c);
}

enum SendResult {
    SUCCESS = 1,
    SUCCESS_CONTINUE,
    ERROR_REUSE_CONNECTION,
    ERROR_DROP_CONNECTION
};

static SendResult WriteHttpResponseHeader(struct lws *wsi,
        HttpResponse* http_response) {
    RTC_DCHECK(http_response != nullptr ) << "HttpResponse is nullptr";
    RTC_DCHECK(http_response->status_ >= 200 ) << "Status is below 200";
    RTC_DCHECK(http_response->mime_.size() > 0 ) << "Mime type is required";

    unsigned char buffer[MAX_SENDBUFFER_SIZE];
    unsigned char *p, *end;
    int n;

    p = buffer + LWS_PRE_SIZE;
    end = p + sizeof(buffer) - LWS_PRE_SIZE;
    if (lws_add_http_header_status(wsi, http_response->status_, &p, end))
        return SendResult::ERROR_REUSE_CONNECTION;

    if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
        (unsigned char *)http_response->mime_.c_str(),
        strlen(http_response->mime_.c_str()), &p, end))
        return SendResult::ERROR_REUSE_CONNECTION;

    if (lws_add_http_header_content_length(wsi,
            http_response->response_.size(), &p, end))
        return SendResult::ERROR_REUSE_CONNECTION;

    if (lws_finalize_http_header(wsi, &p, end))
        return SendResult::ERROR_REUSE_CONNECTION;

    n = lws_write(wsi, buffer + LWS_PRE_SIZE, p - (buffer + LWS_PRE_SIZE),
            LWS_WRITE_HTTP_HEADERS);
    if (n < 0) {
        return SendResult::ERROR_DROP_CONNECTION;
    }
    return SendResult::SUCCESS;
}

static SendResult WriteHttpResponseBody(struct lws *wsi,
        HttpResponse* http_response) {
    RTC_DCHECK(http_response != nullptr );
    RTC_DCHECK(http_response->status_ >= 200 ) << "Status is below 200";
    RTC_DCHECK(http_response->IsHeaderSent() == true )
        << "Header must be sent before body write";

    unsigned char buffer[MAX_SENDBUFFER_SIZE];
    int buffer_size, write_buffer_size, sent ;

    sent = 0;
    do{
        /* we'd like the send this much */
        buffer_size = sizeof(buffer) - LWS_PRE_SIZE;

        /* but if the peer told us he wants less, we can adapt */
        write_buffer_size = lws_get_peer_write_allowance(wsi);

        /* -1 means not using a protocol that has this info */
        if (write_buffer_size == 0) {
            /* right now, peer can't handle anything */
            return SendResult::SUCCESS_CONTINUE;
        }

        if (write_buffer_size != -1 && buffer_size > write_buffer_size )
            /* he couldn't handle that much */
            buffer_size = write_buffer_size;

        if( (unsigned int)buffer_size > http_response->response_.size() -
                http_response->byte_sent_) {
            strcpy( (char *)(buffer+LWS_PRE_SIZE), http_response->response_.c_str());
            buffer_size = http_response->response_.size() -
                http_response->byte_sent_;
        }
        else {
            std::string send_buffer =
                http_response->response_.substr(http_response->byte_sent_,
                    http_response->byte_sent_ + buffer_size);
            strcpy( (char *)(buffer+LWS_PRE_SIZE), send_buffer.c_str() );
        }
        http_response->byte_sent_ += buffer_size;

        /* sent it all */
        if (buffer_size == 0)
            return SendResult::SUCCESS;

        /*
         * To support HTTP2, must take care about preamble space
         *
         * identification of when we send the last payload frame
         * is handled by the library itself if you sent a
         * content-length header
         */
        write_buffer_size = lws_write(wsi, buffer + LWS_PRE_SIZE, buffer_size,
                LWS_WRITE_HTTP);
        if( write_buffer_size < 0 ) {
            RTC_LOG(LS_ERROR) << "write failed in the " <<  __FUNCTION__
                << ", Trying to drop onnection";
            /* write failed, close conn */
		    return SendResult::ERROR_DROP_CONNECTION;
        }
        if( write_buffer_size ) /* while still active, extend timeout */
            lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_CONTENT, 5);
        sent += write_buffer_size;
    } while (!lws_send_pipe_choked(wsi) && (sent < 1024 * 1024));

    return SendResult::SUCCESS_CONTINUE;
}


//////////////////////////////////////////////////////////////////////
//
// Main WebSocket callback for handling file,http,websocket
//
//////////////////////////////////////////////////////////////////////
int LibWebSocketServer::CallbackLibWebsockets(struct lws *wsi,
        enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	struct per_session_data__libwebsockets *pss =
			(struct per_session_data__libwebsockets *)user;
    const char *reason_str;
    const char *mimetype;
    int     sockid;

    sockid = lws_get_socket_fd(wsi);
    if((reason_str = to_callbackreason_str(reason, 1)))
        lwsl_debug("**** %s:: sock(%d),len(%d)\n",
                reason_str, sockid, (int)len );

	switch (reason) {
    //
    // HTTP Callback Processing
    //
    case LWS_CALLBACK_CLOSED_HTTP:
        if( pss != nullptr &&
                pss->userdata_inited == USERDATA_INITED) {
            pss->userdata_inited = USERDATA_UNINITED;
            delete pss->http_request;
            delete pss->http_response;
        };
        break;

	case LWS_CALLBACK_HTTP:
        {
            char buf[512];
            lws_get_peer_simple(wsi, buf, sizeof(buf));

            if( pss->userdata_inited !=  USERDATA_INITED ) {
                // clear whole user data area
                memset( user, 0x00, sizeof(per_session_data__libwebsockets));
                pss->userdata_inited = USERDATA_INITED;
            }

            //  allocate new HttpRequest and HttpResponse
            if( pss->http_request ) {
                delete pss->http_request;
                delete pss->http_response;
            }
            pss->http_request = new HttpRequest();
            pss->http_response = new HttpResponse();

            //
            // Dumping basic request header and information
            if (len < 1) {
                lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);

                /* if we're on HTTP1.1 or 2.0, will keep the idle connection alive */
                if (lws_http_transaction_completed(wsi))
                    return -1;
            }

            //
            // Processing Get Request
            // URI path does not start from root('/'), so fix it at first
            //
            const char root_slash = '/';
            buf[0] =  0x00; // reset buf buffer
            if (*((const char *)in) != root_slash)  strcat(buf, "/");
            strncat(buf, (const char *)in, sizeof(buf) - strlen(buf) - 1);
            // keep the uri_path data in the pss
            strcpy(pss->uri_path, buf);

            RTC_LOG(LS_INFO) << "Internal Http Handler for URI: "
                    << pss->uri_path;
            HttpHandler *handler  = INTERNAL__GET_HTTPHANDLER;
            if(handler != nullptr ){
                BuildHttpRequestFromWsi( wsi, pss->uri_path, pss->http_request );
                if(lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
                    // Post Request type
                    if( pss->http_request->content_length_ > 0 )
                        // neeed to read POST DATA
                        return 0;
                    // Get processng
                    handler->DoPost(pss->http_request, pss->http_response);
                }
                else
                    // Get processng
                    handler->DoGet(pss->http_request, pss->http_response);

                switch(WriteHttpResponseHeader( wsi, pss->http_response )) {
                    case SendResult::SUCCESS:
                        // Sent HttpResponse header successfully
                        pss->http_response->SetHeaderSent();
                        // book us a LWS_CALLBACK_HTTP_WRITEABLE callback
                        lws_callback_on_writable(wsi);
                        return 0;
                    case SendResult::ERROR_REUSE_CONNECTION:
                        return 1;
                    case SendResult::ERROR_DROP_CONNECTION:
                        return -1;
                    default:
                        return -1;
                };
            }

            else {
                // Doing file handling
                std::string mapping_path;
                unsigned char header_buffer[512];
                unsigned char *header_ptr;
                int  header_size = 0, retvalue;

                INTERNAL__GET_WSSINSTANCE ->GetFileMapping(pss->uri_path,mapping_path);
                RTC_LOG(INFO) << "File Request: " << pss->uri_path;

                header_ptr = header_buffer;
                /* refuse to serve files we don't understand */
                mimetype = get_mimetype(mapping_path.c_str());
                if (!mimetype) {
                    RTC_LOG(LS_ERROR) << "Unknown mimetype for " <<  mapping_path.c_str();
                    lws_return_http_status(wsi,
                            HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, NULL);
                    return -1;
                }

                if (lws_is_ssl(wsi) && lws_add_http_header_by_name(wsi,
                            (unsigned char *)
                            "Strict-Transport-Security:",
                            (unsigned char *)
                            "max-age=15768000 ; "
                            "includeSubDomains", 36, &header_ptr,
                            header_buffer +
                            sizeof(header_buffer)))
                    return 1;
                header_size = (unsigned char *)header_ptr - header_buffer;
                retvalue = lws_serve_http_file(wsi,
                        mapping_path.c_str(),
                        mimetype,
                        (const char *)header_buffer,
                        header_size);
                if (retvalue < 0 || ((retvalue > 0) &&
                            lws_http_transaction_completed(wsi)))
                    RTC_LOG(INFO) << "Fail to send: " << mapping_path.c_str();
                    return -1; /* error or can't reuse connection: close the socket */
            }
        }

		break;
	case LWS_CALLBACK_HTTP_WRITEABLE:
        {
            switch( WriteHttpResponseBody(wsi,pss->http_response )) {
                case SUCCESS:
                    // closing http transaction
	                if (lws_http_transaction_completed(wsi)) return -1;
                    return 0;
                case SUCCESS_CONTINUE:
                    // book us a LWS_CALLBACK_HTTP_WRITEABLE callback
		            lws_callback_on_writable(wsi);
                    return 0;
                case ERROR_REUSE_CONNECTION:
                case ERROR_DROP_CONNECTION:
                    RTC_LOG(LS_ERROR) << "Failed to send during WRITEABLE";
                    return -1;
            }
        }
		break;
	case LWS_CALLBACK_HTTP_BODY:
        char internal_buffer[LIBWEBSOCKET_BUFFER_SIZE];
        memcpy(internal_buffer,in, len );
        internal_buffer[len] = 0x00;
        // append data on HttpRequest
        pss->http_request->AddData(internal_buffer);
        RTC_LOG(LS_INFO) << "POST URI Adding Data";
		break;
	case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        {
            char buf[512];
            int  n;
            //
            // Dumping basic request header and information
            dump_handshake_info(wsi);
            /* dump the individual URI Arg parameters */
            n = 0;
            while (lws_hdr_copy_fragment(wsi, buf, sizeof(buf),
                        WSI_TOKEN_HTTP_URI_ARGS, n) > 0) {
                lwsl_notice("URI Arg %d: %s\n", ++n, buf);
            }

            lws_get_peer_simple(wsi, buf, sizeof(buf));
            RTC_LOG(INFO) <<  "HTTP connect from " <<  buf;

            HttpHandler *handler  = INTERNAL__GET_HTTPHANDLER;
            if(handler != nullptr ){
                handler->DoPost(pss->http_request, pss->http_response);

                switch(WriteHttpResponseHeader( wsi, pss->http_response )) {
                    case SendResult::SUCCESS:
                        // Sent HttpResponse header successfully
                        pss->http_response->SetHeaderSent();
                        // book us a LWS_CALLBACK_HTTP_WRITEABLE callback
                        lws_callback_on_writable(wsi);
                        return 0;
                    case SendResult::ERROR_REUSE_CONNECTION:
                        return 1;
                    case SendResult::ERROR_DROP_CONNECTION:
                        return -1;
                    default:
                        return -1;
                };
            }
        }
        break;
	case LWS_CALLBACK_HTTP_FILE_COMPLETION:
        break;
	case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
		break;
	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
		break;
	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		break;
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		break;
	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
		break;
	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		break;
	case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION:
		break;
	case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
		break;

    //
    // Not Used ...
    //
    case LWS_CALLBACK_WSI_CREATE:
	case LWS_CALLBACK_WSI_DESTROY:
	case LWS_CALLBACK_PROTOCOL_INIT:
	case LWS_CALLBACK_PROTOCOL_DESTROY:
	case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
        break;

    //
    // WebSocket Callback Processing
    //
	case LWS_CALLBACK_ESTABLISHED:
        {
            WSInternalHandlerConfig *handler;
            RTC_LOG(INFO) << "LWS_CALLBACK_ESTABLISHED";

            INTERNAL__GET_WEBSOCKETHANDLER
            handler->CreateHandlerRuntime(sockid, wsi);
            handler->OnConnect(sockid);
        }
        lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_CLOSED:
        {
            WSInternalHandlerConfig *handler;
            RTC_LOG(INFO) << __FUNCTION__ << "LWS_CALLBACK_CLOSED";

            INTERNAL__GET_WEBSOCKETHANDLER
            handler->OnDisconnect(sockid);

            for(int index = 0; index < MAX_RINGBUFFER_QUEUE; index++)
                if( pss->ring_buffer[index].payload )
                    free(pss->ring_buffer[index].payload );
        }
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
        {
            WSInternalHandlerConfig *handler;
            std::string message_to_peer;
            int     num_sent,message_length;

            INTERNAL__GET_WEBSOCKETHANDLER

            if( handler->DequeueMessage(sockid, message_to_peer) ) {
                // reset ringbuffer slot used previously
                if( pss->ring_buffer[pss->ring_buffer_index].payload ) {
                    free( pss->ring_buffer[pss->ring_buffer_index].payload);
                    pss->ring_buffer[pss->ring_buffer_index].len = 0;
                };

                message_length =  message_to_peer.length();
                pss->ring_buffer[pss->ring_buffer_index].payload =
                    (char *)malloc(LWS_PRE_SIZE + message_length);
                pss->ring_buffer[pss->ring_buffer_index].len =
                    message_length;
                memcpy( pss->ring_buffer[pss->ring_buffer_index].payload +
                        LWS_PRE_SIZE,
                        message_to_peer.c_str(), message_length );

                num_sent = lws_write(wsi, (unsigned char *)
                        pss->ring_buffer[pss->ring_buffer_index].payload +
                        LWS_PRE_SIZE, message_length, LWS_WRITE_TEXT);
                if (num_sent < 0) {
                    RTC_LOG(LS_ERROR) << "ERROR " << num_sent
                        << " writing to socket for sending meesage to peer";
                    return -1;
                }
                if (num_sent < message_length)
                    RTC_LOG(LS_ERROR) << "ERROR socket partial write " << num_sent
                        << " vs " <<  message_length;

                // increse ring_buffer_index for next usage
			    if (pss->ring_buffer_index == (MAX_RINGBUFFER_QUEUE - 1))
                    pss->ring_buffer_index = 0;
                else
                    pss->ring_buffer_index++;
            };
        }

		break;

	case LWS_CALLBACK_RECEIVE:
        {
            WSInternalHandlerConfig *handler;
            INTERNAL__GET_WEBSOCKETHANDLER

            if ( handler->OnMessage(sockid,std::string((const char *)in)) == false ){
                lws_close_reason (wsi, LWS_CLOSE_STATUS_NORMAL, nullptr, 0);
                return -1;
            }
        }
        lws_callback_on_writable_all_protocol(lws_get_context(wsi),
                lws_get_protocol(wsi));
		break;

	case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
        {
		    dump_handshake_info(wsi);

            if( user ) memset( user, 0x00, sizeof(per_session_data__libwebsockets));
            int len = lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI);
            if (!len || (size_t)len > sizeof(pss->uri_path) - 1) {
                // reject this connection when the URI exceed the uri path buffer size
                lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL,
                    (unsigned char *)"done", 4);
                return -1;
            }
            lws_hdr_copy(wsi, pss->uri_path, sizeof(pss->uri_path), WSI_TOKEN_GET_URI);

            // check whether requested uri is in the he handler config
            if( INTERNAL__GET_WSSINSTANCE ->IsValidWSPath(pss->uri_path) )
		        RTC_LOG(INFO) << "URI Path exist " <<  pss->uri_path;
            else {
		        RTC_LOG(LS_ERROR) << "URI Path does not exist "
                   << pss->uri_path << ", droping this connection\n",
                // reject the connection
		        lws_rx_flow_control(wsi, 0);
                return 1;
            }
        }
		break;

	default:
		break;
	}
	return 0;
}


