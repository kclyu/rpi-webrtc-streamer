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

class PeerConnectionClient {
  constructor(pc_options) {
    // using default pc_options
    this.pc_options_ = pc_options || {
      iceServers: [{ urls: 'stun:stun.l.google.com:19302' }],
    };
    console.log('Created RTCPeerConnnection with config: ', this.pc_options_);

    //  PeerConnection
    this._pc = new RTCPeerConnection(this.pc_options_);
    this._pc.onicecandidate = this._onIceCandidate.bind(this);
    this._pc.ontrack = this._onRemoteStreamAdded.bind(this);
    this._pc.onremovestream = () => {
      console.log('Remote stream removed.');
    };
    this._pc.onsignalingstatechange = this._onSignalingStateChanged.bind(this);
    this._pc.onconnectionstatechange = this._onConnectionStateChanged.bind(
      this
    );
    this._pc.oniceconnectionstatechange = this._onIceConnectionStateChanged.bind(
      this
    );
    this._pc.onnegotiationeeded = () => console.log('Negotiation needed');

    this._startTime = window.performance.now();

    // external public callback
    //
    // The public methods below should be provided where you use
    // the PeerConnectionClient object.
    this.sendSignalingMessage = () => {
      console.error('sendingSignalingMessage callback not specified!');
    };
    this.onRemoteVideoAdded = () => {
      console.error('onRemoveVideoAdded callback not specified!');
    };
    this.onRemoteVideoRemoved = () => {
      console.error('onRemoteVideoRemoved callback not specified!');
    };
    this.onPeerConnectionError = () => {
      console.error('onPeerConnectionError callback not specified!');
    };
  }

  async getUserMedia() {
    if (location.protocol !== 'https:') {
      console.log('Location protocol is not https, Skipping UserMedia');
      return false;
    }
    console.log('Location Requesting local stream');
    try {
      this._localStreams = await navigator.mediaDevices.getUserMedia({
        audio: true,
        video: false,
      });
      console.log('Received local stream', this._localStreams);
      this._localStreams.getTracks().forEach((track) => {
        this._pc.addTrack(track, this._localStreams);
      });
      console.log('Adding Local Stream to peer connection');
    } catch (error) {
      console.log(`getUserMedia() error: ${error}`);
      throw new Error(
        `navigator.MediaDevices.getUserMedia error: ${error.message}, ${error.name}`
      );
    }
  }

  _onIceCandidate(event) {
    if (event.candidate) {
      let message = {
        type: 'candidate',
        label: event.candidate.sdpMLineIndex,
        id: event.candidate.sdpMid,
        candidate: event.candidate.candidate,
      };
      console.log('Sending IceCandidate : ' + JSON.stringify(message));
      this.sendSignalingMessage(message);
    } else {
      console.log('End of candidates.');
    }
  }

  _onIceConnectionStateChanged(_event) {
    console.log(
      'ICE connection state changed to: ' + this._pc.iceConnectionState
    );
    if (
      this._pc.iceConnectionState === 'failed' ||
      this._pc.iceConnectionState === 'disconnected' ||
      this._pc.iceConnectionState === 'closed'
    ) {
      this.onPeerConnectionError(
        'warn',
        'IceStateChanged to ' + this._pc.iceConnectionState
      );
      return;
    }
    if (this._pc.iceConnectionState === 'completed') {
      console.log(
        'ICE complete time: ' +
          (window.performance.now() - this._startTime).toFixed(0) +
          'ms.'
      );
    }
  }

  _onSignalingStateChanged(_event) {
    console.log('Signaling state changed to: ' + this._pc.signalingState);
  }

  _onConnectionStateChanged(_event) {
    console.log('Connection state changed to: ' + this._pc.connectionState);
    if (this._pc.connectionState === 'failed') {
      this.onPeerConnectionError(
        'critical',
        'PeerConnection failed, state:' + this._pc.iceConnectionState
      );
    }
  }

  getPeerConnectionStates() {
    if (!this._pc) {
      return null;
    }
    return {
      signalingState: this._pc.signalingState,
      iceGatheringState: this._pc.iceGatheringState,
      iceConnectionState: this._pc.iceConnectionState,
    };
  }

  getPeerConnectionStats(callback) {
    if (!this._pc) {
      return;
    }
    this._pc.getStats(null).then(callback);
  }

  close() {
    if (this._localStreams) {
      this._localStreams.getTracks().forEach((track) => track.stop());
      this._localStreams = null;
    }

    if (!this._pc) {
      return;
    }

    this._pc.close();
    this._pc = null;
  }

  _onRemoteStreamAdded(event) {
    console.log('Remote stream added:' + event.streams);
    this.onRemoteVideoAdded(event.streams);
  }

  _setLocalSdpAndSendAnswer(sdp) {
    if (!this._pc) {
      return;
    }
    this._pc
      .setLocalDescription(sdp)
      .then(console.log('Set local session description success.'))
      .catch(this._onError.bind(this, 'setLocalDescription'));

    this.sendSignalingMessage(sdp);
  }

  onSignaling(message) {
    console.log('Received signaling message type :' + message['type']);
    if (message['type'] === 'offer') {
      // Processing offer
      this._pc
        .setRemoteDescription(new RTCSessionDescription(message))
        .then(console.log('Set session description success.'))
        .catch(this._onError.bind(this, 'setRemoteDescription'));

      console.log('Creating answer SDP to send peer.');
      this._pc
        .createAnswer()
        .then(this._setLocalSdpAndSendAnswer.bind(this))
        .catch(this._onError.bind(this, 'createAnswer'));
      return;
    } else if (message['type'] === 'candidate') {
      // Processing candidate
      let ice_candidate = new RTCIceCandidate({
        sdpMLineIndex: message.label,
        sdpMid: message.id,
        candidate: message.candidate,
      });

      this._pc
        .addIceCandidate(ice_candidate)
        .then(() => {
          console.log('Remote candidate added successfully.');
        })
        .catch((error) => {
          console.error(`Failed to add Ice Candidate: ${error.toString()}`);
        });
      return;
    }
    console.error('Internal Error, Unknown signaling message: ', message);
  }

  _onError(tag, error) {
    console.error(tag + ': ' + error.toString());
    this.onPeerConnectionError('critical', error.toString());
  }
} // class PeerConnectionClient

export default PeerConnectionClient;
