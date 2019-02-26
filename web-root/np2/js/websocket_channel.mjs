/*
Copyright (c) 2019, rpi-webrtc-streamer Lyu,KeunChang

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

import Constants from './constants.mjs';

///////////////////////////////////////////////////////////////////////////////
//
// WebSocket Channel
//
///////////////////////////////////////////////////////////////////////////////

class WebSocketChannel {

constructor (ws_path) {
    // window.onbeforeunload = this.doSignalingDisconnnect.bind(this);

    if( !window.WebSocket ){
        return new Error('WebSocket is not supported on this platform');
    }

    const websocket_url_path = this.ws_path || Constants.URL_PATH;

    // making WebSocket URL from host
    if( this.isPrivateIPaddress_( location.host ) )
        this.websocket_url_ = "ws://" + location.host + websocket_url_path;
    else
        this.websocket_url_ = "wss://" + location.host + websocket_url_path;

    console.log("RWS connection URL : " + this.websocket_url_);
}

connect ( onmessage, onclose, onerror) {
    return new Promise(function(resolve, reject) {
        if( typeof onmessage !== 'function' ){
            reject(new Error('onmessage message handler is not function'));
        };

        if( this.websocket_ && this.websocket_.readyState === WebSocket.OPEN ){
            reject(new Error('WebSocket instance already exist'));
        };

        console.log('Creating new websocket object');
        this.websocket_ = new WebSocket(this.websocket_url_);

        // resolve when the connnection is established
        this.websocket_.onopen = function() {
            // callback
            this.onmessage_callback_ = onmessage;
            this.onerror_callback_ = onerror;
            this.onclose_callback_ = onclose;

            // callback wrapper
            this.websocket_.onclose = this.onclose_wrapper_.bind(this);
            this.websocket_.onerror = this.onerror_wrapper_.bind(this);
            this.websocket_.onmessage = this.onmessage_wrapper_.bind(this);
            resolve();
        }.bind(this);

        this.websocket_.onerror = function(error) {
            this.websocket_ = null;
            console.error(`Error during connect ${this.websocket_url_},`,
                    error );
            reject(`Error during connect ${this.websocket_url_}`);
        }.bind(this);

    }.bind(this));
}

// Currently, Websocket Channel request method do not support
// multiple request method calls.  Only one request method can be used
// at the same time, and after one request call is finished,
// another request method must be used.
request (data, timeout=2000) {
    // default timeout is 5 seconds
    return new Promise(function(resolve, reject) {
        if ( this.websocket_.readyState !== WebSocket.OPEN) {
            const NotConnectedError =  new Error('Connection is not established.');
            reject(new Error(NotConnectedError));
        };

        this.websocket_.send(data);

        setTimeout(() => {
            reject(new Error('Timed Out.'));
        }, timeout);


        // success in receiving a message response
        this.request_callback_ = (message) => {
            resolve(message);
        };

        this.request_error_callback_ = (error) => {
            reject(new Error(error));
        };

        this.request_close_callback_ = (event) => {
            const ConnectionClosed =  new Error('Connection Closed.');
            reject(new Error(ConnectionClosed));
        };
    }.bind(this));
}

// When send_comand_wrapping is enabled with true, cmd: send is added
// in json message format and the message to be sent is added to
// the msg attribute. If it is disabled with a value of false,
// the message itself is transmitted. The default value is true.
send (message, send_command_wrapping_=true ) {
    if( this.websocket_.readyState == WebSocket.OPEN ){
        if( send_command_wrapping_ ) {
            console.log("C --> WSS: " + message);

            let app_send_message = {
                cmd: 'send',
                msg: JSON.stringify(message)
            };
            let app_message = JSON.stringify(app_send_message);
            if( this.websocket_.send(app_message) === false ) {
                console.error('Failed to send message: ' + app_message);
                return false;
            };
        }
        else {
            if( this.websocket_.send(message) === false ) {
                console.error('Failed to send message: ' + message);
                return false;
            }
        }
        return true;
    }
    console.error('Failed to send message : ' + message);
    throw new Error(
            'Failed to send app message to WSS, Connection is not valid');
}

isConnected () {
    return (this.websocket_.readyState === WebSocket.OPEN );
}


close () {
    if( this.websocket_ ) {
        this.websocket_.close();
    }
    this.websocket_ = null;
}


///////////////////////////////////////////////////////////////////////////////
//
// Wrapper methods
//
///////////////////////////////////////////////////////////////////////////////
onmessage_wrapper_ (event) {

    let dataJson = JSON.parse(event.data);

    //  The cmd response is the cmd keyword used in the request's reply.
    //  Only valid if request_callback exists.
    if( dataJson.cmd === 'response' ) {
        if( this.request_callback_  ){
            this.request_callback_(dataJson);
        }
        else {
            console.error('Invalid response cmd : ' + event.data );
        }
        // Since request callback is disposable,
        // it is deleted after callback call.
        this.reqeust_callback_ = null;
        this.reqeust_close_callback_ = null;
        this.reqeust_error_callback_ = null;
        return;
    }

    // A message that is not a response cmd is a signaling message.
    console.log("WSS -> C: " + event.data);
    if( this.onmessage_callback_ ) {
        this.onmessage_callback_( dataJson );
    }
}

onclose_wrapper_ (event) {
    console.log('Websocket disconnected :', event.code );
    if( this.request_close_callback_  && this.reqeust_callback_ ) {
        // If request callback is set, call request callback first.
        this.request_close_callback_ (event);

        // remove the request callbacks
        this.reqeust_callback_ = null;
        this.reqeust_close_callback_ = null;
        this.reqeust_error_callback_ = null;
    }
    if( this.onclose_callback_ ) {
        this.onclose_callback_( event );
    }
}

onerror_wrapper_(event) {
    console.error("An error occured in websocket: ", event);
    if( this.reqeust_error_callback_  && this.reqeust_callback_ ) {
        // If request callback is set, call request callback first.
        this.reqeust_error_callback_ (event);

        // remove the request callbacks
        this.reqeust_callback_ = null;
        this.reqeust_close_callback_ = null;
        this.reqeust_error_callback_ = null;
    }
    if( this.onerror_callback_ ) {
        this.onerror_callback_( event );
        return;
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// Utility Helper functions
//
///////////////////////////////////////////////////////////////////////////////
isPrivateIPaddress_ (ipaddress) {
    let parts = ipaddress.split('.');
    return parts[0] === '10' ||
        (parts[0] === '172' && (parseInt(parts[1], 10) >= 16 && parseInt(parts[1], 10) <= 31)) ||
        (parts[0] === '192' && parts[1] === '168');
}

}; // class
export default WebSocketChannel;


