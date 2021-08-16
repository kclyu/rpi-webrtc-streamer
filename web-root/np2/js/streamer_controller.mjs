/*
Copyright (c) 2021, rpi-webrtc-streamer Lyu,KeunChang

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
import StreamerConnection from './streamer_connection_rws.mjs';
import SnackbarMessage from './snackbar.mjs';

function updateVideoBackground(url) {
  console.log('Updating video background image : ', url);
  let remoteVideo = document.getElementById(Constants.ELEMENT_VIDEO);
  remoteVideo.style.backgroundImage = `URL('${url}')`;
  remoteVideo.style.backgroundRepeat = 'no-repeat';
  remoteVideo.style.backgroundSize = 'cover';
  remoteVideo.style.backgroundPosition = 'center center';
  /**
   *  FIXME: need to resize the video width/height according to captured image
   */
  /*
  remoteVideo.style.width = '100%';
  remoteVideo.style.height = '100%';

  let actualImage = new Image();
  actualImage.src = url;
  actualImage.onload = function () {
    console.log('Image Actual Size width : ', this.width, 'x', this.height);
  };
  */
}

const ButtonState = {
  enableConnect: 1, // ready to connect, ready to connect streamer
  disableConnect: 2, // enable disconnect, streamer connected
  disableAll: 3, // initial or error state ,
};

class StreamerController {
  constructor() {
    this._streamer_connection = null;
    // destroy created object and manager connection
    window.onbeforeunload = () => this.disconnect(true);
    this.config_media_ = new ConfigMedia();
    this.hookElement();
    this.toggleButton(ButtonState.disableAll);
    this.__reconnect_interval = null;
  }

  get isConnected() {
    return this._streamer_connection && this._streamer_connection.isConnected;
  }

  async _reconnectByInterval() {
    if ((await this.connect()) === true) {
      console.log('reconnection with streamer successful');
      if (this.__reconnect_interval) {
        clearInterval(this.__reconnect_interval);
        this.__reconnect_interval = null;
      }
    } else console.log('reconnection with streamer failed');
  }

  // connecting manager server
  connect() {
    this._streamer_connection = new StreamerConnection({
      onStreamerEvent: this.onStreamerEvent.bind(this),
      onConnectionClose: this.onConnectionError.bind(this),
      onConnectionError: this.onConnectionError.bind(this),
      dump_traffic: true,
    });

    return this._streamer_connection
      .connect()
      .then(() => {
        console.info('connection successful');
        // getting media configuration from streamer
        return this._streamer_connection
          .getMediaConfig()
          .then((media_config) => {
            console.log('Using Media Config : ' + JSON.stringify(media_config));
            this.config_media_.setJsonConfig(media_config);
            this.config_media_.reloadConfigData();
            return true;
          })
          .catch((error) => {
            throw error;
          });
      })
      .then(() => {
        if (this._streamer_connection.isStilCaptureEnabled === true) {
          try {
            // Create an interval to update the background still image
            this.__still_image_update_interval = setInterval(() => {
              if (
                this.isConnected &&
                !this._streamer_connection.isSessionActive()
              ) {
                this._streamer_connection.getStillImage(
                  {},
                  updateVideoBackground
                );
              }
            }, Constants.STILL_IMAGE_UPDATE_INTERVAL);
            // get the still image from rws.
            return this._streamer_connection.getStillImage(
              {},
              updateVideoBackground
            );
          } catch (error) {
            console.trace('Still image capture error: ', error);
          }
        } else {
          return 'still capturing is disabled in RWS';
        }
      })
      .then((result) => {
        console.info('still image result ', result);
        this.toggleButton(ButtonState.enableConnect);
        if (this._streamer_connection.isCameraEnable === false) {
          console.warn(
            'The camera is not enabled in rws, and only audio is supported for streaming.'
          );
        }
        return true;
      })
      .catch((error) => {
        // console.trace('Failed to connect:', error.toString());
        console.error('Failed to connect:', error.toString());
        // if there is any manager connectio between streamer
        // controller will try to reconnet
        SnackbarMessage('Failed to connect streamer: ' + error.toString());
        return false;
      });
  }

  /**
   * clean all objects created by controller
   */
  disconnect(force_disconnect = false) {
    // session related resource
    if (this._streamer_connection) {
      this._streamer_connection.disconnect();
      this._streamer_connection = null;
    }
    // clear still image updater interval
    if (this.__still_image_update_interval) {
      clearInterval(this.__still_image_update_interval);
      this.__still_image_update_interval = null;
    }
    if (force_disconnect && !this.__reconnect_interval) {
      console.log('Start Reconnecting Timer');
      this.__reconnect_interval = setInterval(
        this._reconnectByInterval.bind(this),
        Constants.RECONNECT_INTERVAL
      );
    }
    this.toggleButton(ButtonState.disableAll);
  }

  // event message from streamer
  // TODO: collect whole event message from streamer
  onStreamerEvent(event) {
    if (event.type === 'close') {
      console.log('Event from streamer: ', event.mesg);
      if (this._streamer_connection.isSessionActive()) {
        this._streamer_connection.destroySession();
        SnackbarMessage(`Closing streamer session, connection closed`);
      }
    }
  }

  /**
   * callback handler for streamer connection error
   *
   * @param {('critical'|'warn')} error_level
   * @param error_string = message of error
   */
  onConnectionError(error_level, error_string) {
    if (error_level === 'critical') {
      this.disconnect();
      console.log('Connection Error : ', error_string);
      SnackbarMessage('Connection error: ' + error_string);
    }
    console.trace(`Session Connection Error: ${error_level} : ${error_string}`);
  }

  /**
   * callback handler for RTC PeerConnection error
   *
   * @param {('critical'|'warn')} error_level
   * @param {string} error_string - message of error
   */
  onPeerConnectionError(error_level, error_string) {
    // TODO:  Need to simplify two error handler
    if (error_level === 'critical') {
      SnackbarMessage(error_string);
      this._streamer_connection.destroySession();
      this.toggleButton(ButtonState.enableConnect);
    }
    console.trace(`PeerConnecdtion Error: ${error_level} : ${error_string}`);
  }

  hookElement() {
    // connect and disable button
    this._connectButton = document.getElementById(Constants.ELEMENT_CONNECT);
    this._disconnectButton = document.getElementById(
      Constants.ELEMENT_DISCONNECT
    );
    this._remoteVideo = document.getElementById(Constants.ELEMENT_VIDEO);
    this._connectButton.onclick = () => {
      console.log('Starting streamer session');
      this._streamer_connection.createSession({
        videoElement: Constants.ELEMENT_VIDEO,
        onpeerconnectionerror: this.onPeerConnectionError.bind(this),
      });
      this.toggleButton(ButtonState.disableConnect);
    };
    this._disconnectButton.onclick = () => {
      console.log('Stopping streamer session');
      this._streamer_connection.destroySession();
      this.toggleButton(ButtonState.enableConnect);
    };

    // hook Media configuration element
    if (this.config_media_) {
      this.config_media_.hookConfigElement();
    }

    this._showConfigButton = document.getElementById(
      Constants.ELEMENT_SHOW_CONFIG
    );
    this._remoteVideo = document.getElementById(Constants.ELEMENT_VIDEO);
    this._applyButton = document.getElementById(Constants.ELEMENT_CONFIG_APPLY);
    this._resetButton = document.getElementById(Constants.ELEMENT_CONFIG_RESET);
    this._media_config_section = document.getElementById(
      Constants.ELEMENT_CONFIG_SECTION
    );
    if (this._applyButton)
      this._applyButton.onclick = () => {
        if (this.config_media_ === undefined || this.config_media_ === null) {
          console.error('Internal error Config Media instance is null');
          return;
        }
        this._streamer_connection
          .setMediaConfig(this.config_media_.getJsonConfigElement())
          .then((updated_config) => {
            this.config_media_.setJsonConfig(updated_config);
            return this._streamer_connection.applyMediaConfig();
          })
          .then(() => {
            this.config_media_.reloadConfigData();
            console.log('Applying success');
          })
          .catch((error) => {
            console.error('Failed to set config:', error);
          });
      };
    if (this._resetButton)
      this._resetButton.onclick = this.config_media_.resetToDefault.bind(
        this.config_media_
      );

    this._remoteVideo.addEventListener(
      'mousedown',
      this.handleVideoDragMouseStart.bind(this),
      false
    );
    this._remoteVideo.addEventListener(
      'mouseup',
      this.handleVideoDragEnd.bind(this),
      false
    );
    // this.remoteVideo_.addEventListener('touchmove',
    //         this.handleVideoTouchMove.bind(this), false);
    this._remoteVideo.addEventListener(
      'touchstart',
      this.handleVideoDragTouchStart.bind(this),
      false
    );
    this._remoteVideo.addEventListener(
      'touchend',
      this.handleVideoDragEnd.bind(this),
      false
    );

    document.onkeydown = this.handleVideoZoomReset.bind(this);
  }

  toggleButton(state) {
    switch (state) {
      case ButtonState.enableConnect:
        this._connectButton.disabled = false;
        this._disconnectButton.disabled = true;
        if (this._showConfigButton) {
          this._showConfigButton.disabled = true;
          this._showConfigButton.style.display = 'none';
          if (this._media_config_section)
            this._media_config_section.style.display = 'none';
        }
        break;
      case ButtonState.disableConnect:
        this._connectButton.disabled = true;
        this._disconnectButton.disabled = false;
        if (this._showConfigButton) {
          // most of Button text changed by onclick handler
          // in HTML source.
          // This will forcely change the show button value
          // with default settings after connection established.
          this._showConfigButton.innerText = Constants.ELEMENT_SHOW_BUTTON_TEXT;
          this._showConfigButton.disabled = false;
          this._showConfigButton.style.display = 'block';
        }
        break;
      case ButtonState.disableAll:
        this._connectButton.disabled = true;
        this._disconnectButton.disabled = true;
        if (this._showConfigButton) {
          this._showConfigButton.disabled = true;
          this._showConfigButton.style.display = 'block';
          if (this._media_config_section)
            this._media_config_section.style.display = 'none';
        }
        break;
    }
  }

  handleVideoZoomReset(event) {
    if (!(this._streamer_connection.isConnected && this._remoteVideo.srcObject))
      return;
    if (event.key === 'Escape') {
      console.log('Doing Zoom Resetting');
      this._streamer_connection.sendZoomCommand(0.0, 0.0, 'reset');
    }
  }

  handleVideoDragMouseStart(event) {
    if (!(this._streamer_connection.isConnected && this._remoteVideo.srcObject))
      return;
    // only accept left mouse button
    if (event.which !== 1) {
      this.start_dragX_ = null;
      this.start_dragY_ = null;
      this.start_timeStamp_ = null;
      return;
    }
    this.start_dragX_ = event.clientX;
    this.start_dragY_ = event.clientY;
    this.start_timeStamp_ = event.timeStamp;
  }

  handleVideoDragTouchStart(event) {
    if (!(this._streamer_connection.isConnected && this._remoteVideo.srcObject))
      return;
    if (event.type !== 'touchstart') {
      this.start_dragX_ = null;
      this.start_dragY_ = null;
      this.start_timeStamp_ = null;
      return;
    }
    if (event.changedTouches.length < 1) {
      console.error('changedTouches length is ', event.changedTouches.length);
      return;
    }

    this.start_dragX_ = event.changedTouches[0].clientX;
    this.start_dragY_ = event.changedTouches[0].clientY;
    this.start_timeStamp_ = event.timeStamp;
    console.log('event timestamp: ', event.timeStamp);
  }

  handleVideoDragEnd(event) {
    if (!(this._streamer_connection.isConnected && this._remoteVideo.srcObject))
      return;
    let clientX, clientY, timeStamp;
    if (event.type && event.type === 'touchend') {
      if (event.changedTouches.length < 1) {
        console.error('changedTouches length is ', event.changedTouches.length);
        return;
      }
      clientX = event.changedTouches[0].clientX;
      clientY = event.changedTouches[0].clientY;
      timeStamp = event.timeStamp;
    } else {
      // only accept left mouse button
      if (event.which !== 1) return;
      if (this.start_dragX === null || this.start_dragY === null) return;
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
    let y = clientY - rect.top; // y position within the element.
    let cx = (x / rect.width).toFixed(3); // relative center x/y
    let cy = (y / rect.height).toFixed(3);
    let dx = (dragX / rect.width).toFixed(3); // relative drag X/Y
    let dy = (dragY / rect.height).toFixed(3);

    console.log('difftime: ', parseFloat(timeDiff));
    if (parseFloat(timeDiff) < parseFloat(200)) {
      // regard this as button click
      if (event.shiftKey) {
        this._streamer_connection.sendZoomCommand(cx, cy, 'out');
      } else {
        this._streamer_connection.sendZoomCommand(cx, cy, 'in');
      }
      return;
    } else if (parseFloat(timeDiff) > parseFloat(2000)) {
      // regard this as reset (x,y coordination is meaningless)
      this._streamer_connection.sendZoomCommand(cx, cy, 'reset');
      return;
    } else {
      // regard this as move
      this._streamer_connection.sendZoomCommand(dx, dy, 'move');
    }
  }
} // class StreamerController

export default StreamerController;
