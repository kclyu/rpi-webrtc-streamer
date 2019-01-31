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

const Constants = {
    URL_PATH: '/rws/ws',

    // Element Id for Video related buttons
    ELEMENT_CONNECT: 'Connect',
    ELEMENT_DISCONNECT: 'Disconnect',
    ELEMENT_VIDEO: 'remoteVideo',

    // Element Id for media configurations
    ELEMENT_CONFIG_RESET: 'resetConfig',
    ELEMENT_CONFIG_APPLY: 'applyConfig',
    ELEMENT_CONFIG_SECTION: 'media_configurations',
    ELEMENT_SHOW_CONFIG: 'showConfig',
    ELEMENT_SHOW_BUTTON_TEXT: "Show Video Settings",
    ELEMENT_SHOW_BUTTON_HIDE_TEXT: "Hide Video Settings",

    // Element Id for snackBar message
    ELEMENT_SNACKBAR: 'snackBar',
    SNACKBAR_TIMEOUT: 3000,


    WEBSOCKET_CONNECT_TIMEOUT: 1000,

    // WebSocket request result code
    REQUEST_SUCCESS: 'SUCCESS',
    REQUEST_FAILED: 'FAILED',

    ERROR_INVALID_SIGNAING_CHANNEL: 'Internal Error, Signaling connection is not ready',
    ERROR_INVALID_CALL_CONFIGMEDIA: 'Internal Error, Invalid ConfigMedia method call',
    ERROR_SENDING_REGISTER: 'Internal Error, Sending register command failed',
    ERROR_INVALID_DEVICEID: 'Internal Error, Invalid Device ID',
    ERROR_FAILED_DEVICEID: 'Failed to get Device ID',
    ERROR_FAILED_CONFIG_MEDIA: 'Failed to get Config Media',
    ERROR_FAILED_ZOOM_COMMAND: 'Failed to send Zoom command',

};

export default Constants;


