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
import ConfigMedia from './config_media.mjs';
import PeerConnectionClient from './peerconnection_client.mjs';
import WebSocketChannel from './websocket_channel.mjs';
import SnackBarMessage from './snackbar.mjs';

const ButtonState = {
    enableConnect: 1,   // ready to connect
    disableConnect: 2,  // enable disconnect
    disableAll: 3       // error
};

class StreamerController {

constructor () {
    this._connectButton
        = document.getElementById(Constants.ELEMENT_CONNECT);
    this._disconnectButton
        = document.getElementById(Constants.ELEMENT_DISCONNECT);
    this.remoteVideo_
        = document.getElementById(Constants.ELEMENT_VIDEO);
    if( this._connectButton && this._disconnectButton && this.remoteVideo_ ) {
        // Make sure the necessary resources exist
        this._required_resource = true;

        this._connectButton.onclick = this.connect.bind(this);
        this._disconnectButton.onclick = this.disconnect.bind(this);

        this.websocket_channel_ = null;
        this.peerconnection_client_ = null;
        this.config_media_ = new ConfigMedia();
        this.config_media_inited_ = false;
        this.config_media_.hookConfigElement();

        this.remoteVideo_.addEventListener('mousedown',
                this.handleVideoDragMouseStart.bind(this), false);
        this.remoteVideo_.addEventListener('mouseup',
                this.handleVideoDragEnd.bind(this), false);
        // this.remoteVideo_.addEventListener('touchmove',
        //         this.handleVideoTouchMove.bind(this), false);
        this.remoteVideo_.addEventListener('touchstart',
                this.handleVideoDragTouchStart.bind(this), false);
        this.remoteVideo_.addEventListener('touchend',
                this.handleVideoDragEnd.bind(this), false);

        document.onkeydown = this.handleVideoZoomReset.bind(this);

        this._showConfigButton
            = document.getElementById(Constants.ELEMENT_SHOW_CONFIG);

        this._applyButton
            = document.getElementById(Constants.ELEMENT_CONFIG_APPLY);
        this._resetButton
            = document.getElementById(Constants.ELEMENT_CONFIG_RESET);
        this._media_config_section
            = document.getElementById(Constants.ELEMENT_CONFIG_SECTION);
        if( this._applyButton )
            this._applyButton.onclick = this.applyConfig.bind(this);
        if( this._resetButton )
            this._resetButton.onclick
                = this.config_media_.resetToDefault.bind(this.config_media_);

        // initial button enable/disable values
        this.toggleButton(ButtonState.enableConnect);
        return;
    }
    console.error('The required Element ID does not exist.');
    this.toggleButton(ButtonState.disableAll);
    return;
}

cleanUp () {
    this.remoteVideo_.srcObject = null;

    if( this.websocket_channel_ )
        this.websocket_channel_.close();
    if( this.peerconnection_client_ )
        this.peerconnection_client_.close();

    this.websocket_channel_ = null;
    this.peerconnection_client_ = null;
    this.config_media_inited_ = false;
}

connect() {
    if( !this.websocket_channel_ )
        this.websocket_channel_ = new WebSocketChannel();

    this.websocket_channel_.connect( this.messageHandler_.bind(this),
            this.disconnect.bind(this))
        .then(() => {
        console.log('WebSocket Connected');
    })
    .then(() => {
        return this.getDeiceId();
    })
    .then(() => {
        return this.getConfigMedia().then((result) => {
            if( !this.config_media_ ) {
                console.error('Internal error Config Media instance is null');
            } else {
                console.log('Using Media Config : ' + JSON.stringify(result));
                this.config_media_.setJsonConfig(result);
                this.config_media_.reloadConfigData();
                this.config_media_inited_ = true;
            }
        });
    })
    .then(() => {
        return this.getRTCConfig().then((result) => {
            console.log('Using RTCConfig : ' + JSON.stringify(result));

            if( !this.peerconnection_client_ )
                this.peerconnection_client_ = new PeerConnectionClient(result);

            // providiing peerconnectionclient public method
            this.peerconnection_client_.sendSignalingMessage =
                this.websocket_channel_.send.bind(this.websocket_channel_);
            this.peerconnection_client_.onRemoteVideoAdded =
                this.hookVideoElement.bind(this);
            this.peerconnection_client_.onPeerConnectionError =
                this.handlePeerConnectionError.bind(this);

            // disable connect button
            this.toggleButton(ButtonState.disableConnect);
        });
    })
    .then(() => {
        console.log('Location Protocol: ' + location.protocol);
        if( location.protocol === "https:") {
            console.log('Trying to get UserMedia');
            return this.peerconnection_client_.getUserMedia();
        }
    })
    .then(() => {
        return this.sendRegister().then(() => {
            console.log('Sending regsiter command successful');
        });
    })
    .catch((error) => {
        console.error('Failed to connect:', error);
        SnackBarMessage(error.toString());
        this.disconnect();
    });
}

disconnect () {
    console.log('Disconnecting');
    this.toggleButton(ButtonState.enableConnect);
    this.cleanUp();
}

applyConfig () {
    if( this.config_media_ === undefined ||
            this.config_media_ === null ) {
        console.error('Internal error Config Media instance is null');
        return;
    };
    this.setConfigMedia().then((result) => {
        this.config_media_.setJsonConfig(result);
        return this.applyConfigMedia();
    })
    .then(() => {
        this.config_media_.reloadConfigData();
        console.log('Applying success');
    })
    .catch( (error) => {
        console.error('Failed to set config:', error);
    });
}

messageHandler_ (message) {
    // console.log('Received message : ' + JSON.stringify(message));
    // send type command
    if( message['cmd'] === 'send' ) {
        try {
            let JsonMsg = JSON.parse(message['msg']);
            this.peerconnection_client_
                .receivedSignalingMessage( JsonMsg );
        }
        catch(error){
            console.error('Parsing error in send command : '
                    + error.toString());
        }
    }
    else if( message['cmd'] === 'event' ) {
        console.log('Event message from WSS: ',
                JSON.stringify(message));
        if( message['type'] === 'error' )
            alert(JSON.stringify(message));
    }
}

hookVideoElement (streams) {
    if ( this.remoteVideo_.srcObject !== streams[0]) {
        this.remoteVideo_.srcObject = streams[0];
        console.log('PeerConnection received remote stream');
    }

}

handlePeerConnectionError( error_msg ) {
    console.log(`PeerConnction Error  : ${error_msg}`);
}

handleVideoZoomReset (event) {
    if (event.key === "Escape") {
        if( this.isSignalingChannelValid() === false ) {
            return;
        };
        console.log('Doing Zoom Resetting');
        this.sendZoomCommand( 0.0, 0.0, 'reset');
    }
}

handleVideoDragMouseStart (event) {
    // only accept left mouse button
    if( event.which !== 1) {
        this.start_dragX_ = null;
        this.start_dragY_ = null;
        this.start_timeStamp_ = null;
        return;
    }
    this.start_dragX_ = event.clientX;
    this.start_dragY_ = event.clientY;
    this.start_timeStamp_ = event.timeStamp;
}

handleVideoDragTouchStart (event) {
    // only accept left mouse button
    if( event.type !== 'touchstart') {
        this.start_dragX_ = null;
        this.start_dragY_ = null;
        this.start_timeStamp_ = null;
        return;
    }
    if( event.changedTouches.length < 1) {
        console.error('changedTouches length is ', event.changedTouches.length );
        return
    }

    this.start_dragX_ = event.changedTouches[0].clientX;
    this.start_dragY_ = event.changedTouches[0].clientY;
    this.start_timeStamp_ = event.timeStamp;
}

handleVideoDragEnd (event) {
    let clientX, clientY, timeStamp;
    if( event.type && event.type === 'touchend') {
        if( event.changedTouches.length < 1) {
            console.error('changedTouches length is ', event.changedTouches.length );
            return
        }
        clientX = event.changedTouches[0].clientX;
        clientY = event.changedTouches[0].clientY;
        timeStamp = event.timeStamp;
    }
    else {
        // only accept left mouse button
        if( event.which !== 1) return;
        if( this.start_dragX === null || this.start_dragY === null ) return;
        clientX = event.clientX;
        clientY = event.clientY;
        timeStamp = event.timeStamp;
    }

    let dragX = this.start_dragX_ - clientX;
    let dragY = this.start_dragY_ - clientY;
    let timeDiff = timeStamp - this.start_timeStamp_;

    // get the position inside the video element
    let rect = event.target.getBoundingClientRect();
    let x = clientX - rect.left; // x position within the element.
    let y = clientY - rect.top;  // y position within the element.
    let cx =  (x/rect.width).toFixed(3); // relative center x/y
    let cy =  (y/rect.height).toFixed(3);
    let dx =  (dragX/rect.width).toFixed(3);    // relative drag X/Y
    let dy =  (dragY/rect.height).toFixed(3);

    if( parseFloat(timeDiff) < parseFloat(200) ) {
        // regard this as button click
        this.sendZoomCommand( cx, cy, 'in');
        return;
    }
    else if( parseFloat(timeDiff) > parseFloat(2000) ) {
        // regard this as reset (x,y coordination is meaningless)
        this.sendZoomCommand( cx, cy, 'reset');
        return
    }
    else {
        // regard this as move
        this.sendZoomCommand( dx, dy, 'move');
    }
}

toggleButton (state) {
    switch( state ) {
        case ButtonState.enableConnect:
            this._connectButton.disabled = false;
            this._disconnectButton.disabled = true;
            if( this._showConfigButton ) {
                this._showConfigButton.disabled = true;
                this._showConfigButton.style.display = 'none';
                if( this._media_config_section )
                    this._media_config_section.style.display = 'none';
            }
            break;
        case ButtonState.disableConnect:
            this._connectButton.disabled = true;
            this._disconnectButton.disabled = false;
            if( this._showConfigButton ) {
                // most of Button text changed by onclick handler
                // in HTML source.
                // This will forcely change the show button value
                // with default settings after connection established.
                this._showConfigButton.innerText
                    = Constants.ELEMENT_SHOW_BUTTON_TEXT;
                this._showConfigButton.disabled = false;
                this._showConfigButton.style.display = 'block';
            }
            break;
        case ButtonState.disableAll:
            this._connectButton.disabled = true;
            this._disconnectButton.disabled = true;
            if( this._showConfigButton ) {
                this._showConfigButton.disabled = true;
                this._showConfigButton.style.display = 'block';
                if( this._media_config_section )
                    this._media_config_section.style.display = 'none';
            }
            break;
    }
}

isSignalingChannelValid () {
    if( this.websocket_channel_ && this.websocket_channel_.isConnected() )
        return true;
    return false;
}

async sendRegister () {
    // No Room concept, generate random room and client id.
    let register = { cmd: 'register',
        roomid: this._randomString(9),
        clientid: this._randomString(8)
    };
    let register_message = JSON.stringify(register);

    if( this.websocket_channel_.send(register_message,false) === false ) {
        console.error('Failed to send register message: ' + register_message);
        return new Error(Constants.ERROR_SENDING_REGISTER);
    }
    return true;
}

async sendZoomCommand (cx, cy, action) {
    // Sending zoom command vis websocket message command
    if( parseFloat(cx) > 1.0 || parseFloat(cy) > 1.0 ){
        console.error('Center x/y value error: ' + cx + ',' + cy );
        return;
    }
    let cmd = { cmd: 'message', type: 'zoom' };
    let zoom = {};
    zoom['x'] = cx;
    zoom['y'] = cy;
    zoom['command'] = action;   // in/out/reset
    cmd['data'] = JSON.stringify(zoom);

    let cmd_message = JSON.stringify(cmd);
    if( this.websocket_channel_.send(cmd_message,false) === false ) {
        console.error('Failed to send register message: ' + cmd_message);
        return new Error(Constants.ERROR_FAILED_ZOOM_COMMAND);
    }
    return true;
}

async getDeiceId () {
    if( this.isSignalingChannelValid() === false ) {
        return new Error(Constants.ERROR_INVALID_SIGNAING_CHANNEL);
    }

    let request = {};
    request['cmd'] = 'request';
    request['type'] = 'deviceid';

    let response = await this.websocket_channel_.request(JSON.stringify(request));
    if(response.result === Constants.REQUEST_SUCCESS ) {
        let resp_data = JSON.parse(response.data);
        this.connected_ = true;
        this.deviceid_ = resp_data['deviceid'];
        this.mcversion_ = resp_data['mcversion'];
        console.log(`RWS Device ID : ${this.deviceid_}, MC Version: ${this.mcversion_}`);
        return(resp_data);
    }
    else {
        console.error('Response Result: ', response );
        return new Error(Constants.ERROR_FAILED_DEVICEID);
    }
}

async getConfigMedia () {
    if( this.isSignalingChannelValid() === false ) {
        return new Error(Constants.ERROR_INVALID_SIGNAING_CHANNEL);
    };
    if( !this.deviceid_ ) {
        return new Error(Constants.ERROR_INVALID_DEVICEID);
    };

    let request = {};
    request['cmd'] = 'request';
    request['type'] = 'config';
    request['deviceid'] = this.deviceid_;
    request['data'] = 'read';
    let response = await this.websocket_channel_.request(JSON.stringify(request));
    if(response.result === Constants.REQUEST_SUCCESS ) {
        let resp_data = JSON.parse(response.data);
        // console.log('Config Data: ' + response.data);
        return(resp_data);
    }
    else {
        console.error('Response Error: ', response );
        return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
    }
}

async setConfigMedia () {
    if( this.isSignalingChannelValid() === false ) {
        return new Error(Constants.ERROR_INVALID_SIGNAING_CHANNEL);
    };
    if( !this.deviceid_ ) {
        return new Error(Constants.ERROR_INVALID_DEVICEID);
    };

    let request = {};
    request['cmd'] = 'request';
    request['type'] = 'config';
    request['deviceid'] = this.deviceid_;
    request['data'] = JSON.stringify(this.config_media_.getJsonConfigElement());
    let response = await this.websocket_channel_.request(JSON.stringify(request));
    console.log('setConfigMedia Result: ', response );
    if(response.result === Constants.REQUEST_SUCCESS ) {
        let resp_data = JSON.parse(response.data);
        console.log('Config Data: ' + response.data);
        return(resp_data);
    }
    else {
        console.error('Response Error: ', response );
        return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
    }
}

async applyConfigMedia () {
    if( this.isSignalingChannelValid() === false ) {
        return new Error(Constants.ERROR_INVALID_SIGNAING_CHANNEL);
    };
    if( !this.deviceid_ ) {
        return new Error(Constants.ERROR_INVALID_DEVICEID);
    };

    let request = {};
    request['cmd'] = 'request';
    request['type'] = 'config';
    request['deviceid'] = this.deviceid_;
    request['data'] = 'apply';
    let response = await this.websocket_channel_.request(JSON.stringify(request));
    console.log('apply Result: ', response );
    if(response.result === Constants.REQUEST_SUCCESS ) {
        return;
    }
    else {
        console.error('Response Error: ', response );
        return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
    }
}

async getRTCConfig () {
    if( this.isSignalingChannelValid() === false ) {
        return new Error(Constants.ERROR_INVALID_SIGNAING_CHANNEL);
    };
    if( !this.deviceid_ ) {
        return new Error(Constants.ERROR_INVALID_DEVICEID);
    };

    let request = {};
    request['cmd'] = 'request';
    request['type'] = 'rtcconfig';
    request['deviceid'] = this.deviceid_;
    let response = await this.websocket_channel_.request(JSON.stringify(request));
    if(response.result === Constants.REQUEST_SUCCESS ) {
        let resp_data = JSON.parse(response.data);
        // console.log('RTC Config: ', response.data );
        return(resp_data);
    }
    else  {
        console.error('Response Error: ', response );
        return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
    }
}

_randomString (length) {
    // Return a random numerical string.
    let result = [];
    let strLength = length || 5;
    let charSet = '0123456789';
    while (strLength--) {
        result.push(charSet.charAt(Math.floor(Math.random() * charSet.length)));
    }
    return result.join('');
}

}; // class StreamerController
export default StreamerController;


