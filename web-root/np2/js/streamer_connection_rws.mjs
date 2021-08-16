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
import Transaction from './streamer_transaction.mjs';
import PeerConnectionClient from './peerconnection_client.mjs';

const noop = () => {}; // no operation
const StreamerAllowedRequests = [
  // 'state',
  // 'serverinfo',
  'info',
  'deviceid',
  'config',
  'rtcconfig',
  'mediaconfig',
  'still',
];

class StreamerRwsConnection {
  constructor(params) {
    if (!window.WebSocket) {
      throw new Error('WebSocket is not supported on this platform');
    }
    this._connection = null;
    // making default connection url
    this._default_websocket_url =
      (location.protocol === 'https:' ? 'wss:' : 'ws:') +
      location.host +
      (params.wspath || Constants.URL_PATH);

    this._streamer_session = null;
    this._transaction_map = new Map();

    // message handling callbacks
    this._onConnect = params.onConnect || noop;
    this._onStreamerEvent = params.onStreamerEvent || noop;
    this._onClose = params.onConnectionClose || noop;
    this._onError = params.onConnectionError || noop;

    // dump traffic on console
    this._dump_traffic = params.dump_traffic === true || false;

    // device info which is retrieved from device through getDeviceInfo
    this._device_info = null;
  }

  get isConnected() {
    return this._connection && this._connection.readyState === WebSocket.OPEN;
  }

  get isStilCaptureEnabled() {
    return (
      this.isConnected && this._device_info && this._device_info.stillcapture
    );
  }

  get isCameraEnabled() {
    return (
      this.isConnected && this._device_info && this._device_info.cameraenabled
    );
  }

  connect(url) {
    return new Promise((resolve, reject) => {
      this._websocket_url = url || this._default_websocket_url;
      console.log('Trying to connect rws, URL : %s', this._websocket_url);
      if (this._connection) {
        console.warn('Resetting rws connection!');
        this.disconnect();
      }
      this._connection = new WebSocket(this._websocket_url);
      this._connection.onerror = (error) => {
        this._connection = null;
        reject(error);
      };

      this._connection.onopen = () => {
        console.log('Streamer rws connection connected');
        this._connection.onmessage = this.onMessage.bind(this);
        this._connection.onclose = this.onClose.bind(this);
        this._connection.onerror = this.onError.bind(this);
        // calling onConnect callback
        this._onConnect();
        this.getDeviceInfo();
        resolve();
      };
    });
  }

  // destroy and clean whole object which is created during session
  // initialization and creation. used to terminate this sesion
  // and need to create new streamsession
  disconnect() {
    console.trace('Disconnecting connection');
    // reset callback
    this._onStreamerEvent = null;
    this._onConnect = null;
    this._onClose = null;
    this._onError = null;

    this._transaction_map.clear();
    if (this._streamer_session) {
      if (this._streamer_session.remoteVideo)
        this._streamer_session.remoteVideo.srcObject = null;
      if (this._streamer_session.pcclient) {
        this._streamer_session.pcclient.onRemoteVideoAdded = null;
        this._streamer_session.pcclient.close();
        this._streamer_session.pcclient = null;
      }
      this._streamer_session = null;
    }
    if (this._connection) {
      // suppress close event
      this._connection.onclose = null;
      this._connection.close();
      this._connection = null;
    }

    // clear the device info
    this._device_info = null;
  }

  async createSession(params) {
    console.assert(params.videoElement, 'missing videoElement in params');

    let session = { videoElement: params.videoElement };
    session.pcclient = new PeerConnectionClient(
      await this.getRTCConfig()
        .then((data) => {
          return JSON.parse(data);
        })
        .catch((error) => {
          console.trace('Error in getting RTC configuration, using default');
          return null;
        })
    );
    // setup callbacks for PeerConnection message handshacking
    session.pcclient.sendSignalingMessage =
      this.sendSignalingMessage.bind(this);
    session.remoteVideo = document.getElementById(session.videoElement);
    console.assert(
      session.remoteVideo,
      `videoElement ${session.videoElement} not found in document`
    );
    session.pcclient.onRemoteVideoAdded = (streams) => {
      if (session.remoteVideo.srcObject !== streams[0]) {
        session.remoteVideo.srcObject = streams[0];
        console.log('PeerConnection received remote video stream');
      }
    };
    session.pcclient.onPeerConnectionError = params.onpeerconnectionerror
      ? params.onpeerconnectionerror
      : noop;

    session.pcclient
      .getUserMedia()
      .then(() => {
        return this.sendRegister();
      })
      .then(() => {
        this._streamer_session = session;
        console.log('RWS WebRTC streaming session created');
      })
      .catch((error) => {
        console.log('Failed to create RWS WebRTC session ');
        if (session.pcclient) {
          session.pcclient.onRemoteVideoAdded = null;
          session.pcclient.close();
          session.pcclient = null;
        }
        if (session.remoteVideo) session.remoteVideo.srcObject = null;
        const error_mesg = `Failed to create session : ${error}`;
        console.error(error_mesg);
        if (typeof params.onpeerconnectionerror === 'function')
          params.onpeerconnectionerror('critical', error_mesg);
      });
  }

  destroySession() {
    // clean the objects created in StreamingSession.
    this.sendBye()
      .then((resp) => {
        console.log(`destroying streamer session successful`);
      })
      .catch((error) => {
        console.error(`Failed to destroy session: ${error}`);
      });

    // clean streaming session regardless of the result of bye comand success
    if (this._streamer_session) {
      if (this._streamer_session.pcclient) {
        this._streamer_session.pcclient.close();
        this._streamer_session.pcclient = null;
      }
      this._streamer_session.remoteVideo.srcObject = null;
    }
  }

  isSessionActive() {
    return (
      this.isConnected &&
      this._streamer_session &&
      this._streamer_session.pcclient
    );
  }

  //
  // Request : { cmd: 'request',
  //   type: StreamerAllowedRequests,
  //   transaction: '...'
  //   data: {...}
  //   result: {'SUCCESS'|'FAILED'}
  //   error: '...error message' }
  //
  sendRequest(req) {
    if (
      req.cmd !== 'request' ||
      StreamerAllowedRequests.indexOf(req.type) === -1
    ) {
      throw new Error('Invalid request params: ', req);
    }

    const req_promise = new Promise((resolve, reject) => {
      let transaction = new Transaction(this._transaction_map, resolve, reject);
      this._transaction_map.set(transaction.id, transaction);
      // adding transaction id in request
      req.transaction = transaction.id;

      try {
        if (this._dump_traffic) console.debug('C -> WS: ', JSON.stringify(req));
        if (this.isConnected) this._connection.send(JSON.stringify(req));
        else throw new Error('rws connection is not conncted.');
      } catch (error) {
        throw error;
      }
    });
    return req_promise;
  }

  sendSignalingMessage(mesg) {
    try {
      if (this._dump_traffic)
        console.debug('Signaling C -> WS: ', JSON.stringify(mesg));
      if (this.isConnected)
        this._connection.send(
          JSON.stringify({ cmd: 'send', msg: JSON.stringify(mesg) })
        );
      else throw new Error('rws connection is not conncted.');
    } catch (error) {
      throw error;
    }
  }

  onMessage(event) {
    try {
      const data = JSON.parse(event.data);
      // Handling 'response'
      if (data.cmd === 'response' && data.transaction) {
        if (this._dump_traffic) console.debug('WS -> C: ', data);
        // the received message is response
        const transaction = this._transaction_map.get(data.transaction);
        if (data.result === 'SUCCESS') transaction.success(data);
        else if (data.result === 'FAILED') transaction.fail(data.error);
        else transaction.fail('Internal error,unknown data result code');
        this._transaction_map.delete(data.transaction);
        return;
      } else if (data.cmd === 'send') {
        if (this._dump_traffic)
          console.debug('Signaling WS -> C: ', JSON.stringify(data));
        // data.msg is JSON string format, so need to parse it again
        this.onSignaling(JSON.parse(data.msg));
        return;
      } else if (data.cmd === 'event') {
        if (this._dump_traffic) console.debug('Event WS -> C: ', data);
        // passing the whole event mesg
        this._onStreamerEvent(data);
        return;
      }
      console.error('Unhandled message: ', data);
    } catch (error) {
      console.error('Error in recevied message: ', error);
    }
  }

  //
  onSignaling(mesg) {
    console.log('onSignailing Mesg: ', mesg);
    try {
      this._streamer_session.pcclient.onSignaling(mesg);
      return;
    } catch (error) {
      console.error('Error on signaling message :', error);
    }
  }

  onError(event) {
    console.error('Error occured in rws connection: ', event.error);
    this._onError('critical', `Connection Error : ${event.error}`);
  }

  onClose(event) {
    console.error('rws connection closed: ', event.code, event.reason);
    this._onClose('critical', `Connection closed code ${event.code}`);
  }

  /**
   * Get the device information from streamer
   */
  async getDeviceInfo() {
    try {
      const response = await this.sendRequest({
        cmd: 'request',
        type: 'info',
      });
      // keep the device information
      this._device_info = JSON.parse(response.data);
      console.log(
        `streamer device id ${this._device_info.deviceid}, `,
        `mcversion: ${this._device_info.mcversion},`,
        `still capture: ${this._device_info.stillcapture},`,
        `camera enabled: ${this._device_info.cameraenabled}`
      );
    } catch (error) {
      console.trace('Failed to get media configuration from streamer: ', error);
      return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
    }
  }

  /**
   * send register signaling message
   */
  async sendRegister() {
    if (this.isConnected === false) {
      return new Error(Constants.ERROR_CONNECTION_NOT_ESTABLISHED);
    }
    let mesg = {
      cmd: 'register',
      roomid: this._randomString(9),
      clientid: this._randomString(8),
    };
    try {
      this._connection.send(JSON.stringify(mesg));
    } catch (error) {
      console.error('Failed to send register message: ', mesg);
      throw new Error(Constants.ERROR_SENDING_REGISTER);
    }
  }

  /**
   * Send bye signaling message
   */
  async sendBye() {
    if (this.isConnected === false) {
      return new Error(Constants.ERROR_CONNECTION_NOT_ESTABLISHED);
    }
    let mesg = {
      cmd: 'send',
      msg: JSON.stringify({ type: 'bye' }),
    };
    try {
      this._connection.send(JSON.stringify(mesg));
    } catch (error) {
      console.error('Failed to send bye message: ', mesg);
      throw error;
    }
  }

  /**
   * Get RTCConfiguration from straemer configuration
   * @returns {string} - json string format of RTCConfiguration
   * @throws - throw error if there is error in connection or failed to get RTCconfiguration
   */
  async getRTCConfig() {
    if (this.isConnected === false) {
      return new Error(Constants.ERROR_CONNECTION_NOT_ESTABLISHED);
    }
    let request = {
      cmd: 'request',
      type: 'rtcconfig',
    };
    try {
      const response = await this.sendRequest(request);
      return response.data;
    } catch (error) {
      console.trace('Failed to get RTC configuration from streamer');
      throw error;
    }
  }

  /**
   * Get Media configuration from straemer configuration
   * @returns {Object} - JSON object of Media configuration
   * @throws - throw error if there is error in connection or failed to get Media configuration
   */
  async getMediaConfig() {
    if (this.isConnected === false) {
      return new Error(Constants.ERROR_CONNECTION_NOT_ESTABLISHED);
    }
    let request = {
      cmd: 'request',
      type: 'config',
      data: 'read',
    };
    try {
      const response = await this.sendRequest(request);
      return JSON.parse(response.data);
    } catch (error) {
      console.trace('Failed to get media configuration from streamer: ', error);
      return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
    }
  }

  /**
   * Set Media configuration to straemer configuration
   * The changed media config is sent to the streamer so that the changed media
   * config is used. The changed setting is valid until it is changed again,
   * and the changed setting disappears when the streamer is restarted.
   *
   * @param {Object} data - JSON Object of Media configuration
   * @returns {Object} - json object of Media configuration
   * @throws - throw error if there is error in connection or failed to set Media configuration
   */
  async setMediaConfig(config = {}) {
    if (this.isConnected === false) {
      return new Error(Constants.ERROR_INVALID_SIGNAING_CHANNEL);
    }

    let request = {
      cmd: 'request',
      type: 'config',
      data: JSON.stringify(config),
    };
    try {
      let response = await this.sendRequest(request);
      if (response.result === Constants.REQUEST_SUCCESS) {
        return JSON.parse(response.data);
      } else {
        console.error('Failed to set Media configration error: ', response);
        return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
      }
    } catch (error) {
      console.trace('Failed to get media configuration from streamer: ', error);
      return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
    }
  }

  /**
   * Apply updated Media configuration in streamer
   *
   * @returns {boolean}
   * @throws - throw error if there is error in connection or failed to apply Media configuration
   */
  async applyMediaConfig() {
    if (this.isConnected === false) {
      return new Error(Constants.ERROR_INVALID_SIGNAING_CHANNEL);
    }

    let request = {
      cmd: 'request',
      type: 'config',
      data: 'apply',
    };
    try {
      let response = await this.sendRequest(request);
      if (response.result === Constants.REQUEST_SUCCESS) {
        return true;
      } else {
        console.error('Failed to set Media configration error: ', response);
        return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
      }
    } catch (error) {
      console.trace('Failed to get media configuration from streamer: ', error);
      return new Error(Constants.ERROR_FAILED_CONFIG_MEDIA);
    }
  }

  /**
   * Send zoom command message
   * @param {string} x - float value of string type
   * @param {string} y - float value of string type
   * @param {('in'|'out'|'reset'|'move')} cmd - zoom command
   * @throws - throw error if there is error in connection or failed to send zoom command
   */
  async sendZoomCommand(x, y, cmd) {
    if (this.isConnected === false) {
      return new Error(Constants.ERROR_CONNECTION_NOT_ESTABLISHED);
    }
    // Sending zoom command vis websocket message command
    if (parseFloat(x) > 1.0 || parseFloat(y) > 1.0) {
      return new Error(`Error in x/y value error: ${x},${y}`);
    }
    if (parseFloat(x) < 0 || parseFloat(y) < 0) {
      return new Error(`Error in x/y value error: ${x},${y}`);
    }
    let zoom = {
      x: x,
      y: y,
      command: cmd,
    };
    let mesg = { cmd: 'message', type: 'zoom', data: JSON.stringify(zoom) };
    try {
      this._connection.send(JSON.stringify(mesg));
    } catch (error) {
      console.trace('Failed to get media configuration from streamer: ', error);
      return new Error(Constants.ERROR_FAILED_ZOOM_COMMAND);
    }
  }

  /**
   * callback function typedef used as a function parameter in Still Capture.
   *
   * @callback captureCallback - callback for handle capture result
   * @param url - url path of captured file
   */

  /**
   * @param {Object} options - custom options to capture
   * @param {number} [options.width] - width of still image
   * @param {number} [options.height] - height of still image
   * @param {number} [options.quality] - quality of still image
   * @param {('jpg'|'png'|'bmp'|'gif')} [options.extenstion] - still image encoding
   * @param {captureCallback} [callback=null] - callback to handle capture result
   * @returns {Object} captureResult - file name and the url path
   * @returns {string} captureResult.filename - the filename of captured image
   * @returns {string} captureResult.url - url path
   */
  async getStillImage(options = {}, callback = null) {
    if (this.isConnected === false) {
      return new Error(Constants.ERROR_CONNECTION_NOT_ESTABLISHED);
    }
    let req = {
      cmd: 'request',
      type: 'still',
      data: options,
    };
    try {
      const resp = await this.sendRequest(req);
      const resp_data = JSON.parse(resp.data);
      if (callback && typeof callback === 'function') {
        console.log('Calling StillImage callback with url: ', resp_data.url);
        callback(resp_data.url);
      }
      return resp_data;
    } catch (error) {
      throw error;
    }
  }

  _randomString(length) {
    // Return a random numerical string.
    let result = [];
    let strLength = length || 5;
    let charSet = '0123456789';
    while (strLength--) {
      result.push(charSet.charAt(Math.floor(Math.random() * charSet.length)));
    }
    return result.join('');
  }
} // class StreamerRwsSession

export default StreamerRwsConnection;
