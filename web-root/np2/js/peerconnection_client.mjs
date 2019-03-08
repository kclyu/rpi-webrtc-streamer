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


class PeerConnectionClient {

constructor(pc_options) {
    if( pc_options )
        this.pc_options_ = pc_options;
    else
        // using default pc_options
        this.pc_options_ = {'iceServers': [{'urls': 'stun:stun.l.google.com:19302'}]};

    //  PeerConnection
    this.pc_ = new RTCPeerConnection(this.pc_options_);
    this.pc_.onicecandidate = this.onIceCandidate_.bind(this);
    this.pc_.ontrack = this.onRemoteStreamAdded_.bind(this);
    this.pc_.onremovestream = () => {console.log('Remote stream removed.')}
    this.pc_.onsignalingstatechange = this.onSignalingStateChanged_.bind(this);
    this.pc_.oniceconnectionstatechange =
        this.onIceConnectionStateChanged_.bind(this);
    this.pc_.onnegotiationeeded = () => console.log('Negotiation needed');

    console.log('Created RTCPeerConnnection with config: ' + JSON.stringify(this.pc_options_));
    window.dispatchEvent(new CustomEvent('pccreated', {
        detail: {
            pc: this,
            time: new Date(),
        }
    }));
    this.startTime_ = window.performance.now();

    // external public callback
    //
    // The public methods below should be provided where you use
    // the PeerConnectionClient object.
    this.sendSignalingMessage = null;
    this.onRemoteVideoAdded = null;
    this.onPeerConnectionError = null;
}

async getUserMedia() {
    console.log('Requesting local stream');
    try {
        this.localStreams_ = await navigator.mediaDevices.getUserMedia({audio: true, video: false});
        console.log('Received local stream', this.localStreams_);
        this.localStreams_.getTracks().forEach( track => {
                    this.pc_.addTrack( track, this.localStreams_);
                });
        console.log('Adding Local Stream to peer connection');
    } catch (error) {
        console.log(`getUserMedia() error: ${error}`);
        throw new Error(`navigator.MediaDevices.getUserMedia error: ${error.message}, ${error.name}`);
    }
}

onIceCandidate_ (event) {
    if (!this.pc_) {
        return;
    }
    if (event.candidate) {
        let message = {
            type: 'candidate',
            label: event.candidate.sdpMLineIndex,
            id: event.candidate.sdpMid,
            candidate: event.candidate.candidate
        };
        console.log('Sending IceCandidate : ' + JSON.stringify(message));
        if( this.sendSignalingMessage )
            // this.sendSignalingMessage( JSON.stringify(message));
            this.sendSignalingMessage(message);
    } else {
        console.log('End of candidates.');
    }
}

onIceConnectionStateChanged_ () {
    if (!this.pc_) {
        return;
    }
    console.log('ICE connection state changed to: ' + this.pc_.iceConnectionState);
    if (this.pc_.iceConnectionState === 'completed') {
        console.log('ICE complete time: ' +
                (window.performance.now() - this.startTime_).toFixed(0) + 'ms.');
    }
}

onSignalingStateChanged_ () {
    if (!this.pc_) {
        return;
    }
    console.log('Signaling state changed to: ' + this.pc_.signalingState);
}

getPeerConnectionStates () {
    if (!this.pc_) {
        return null;
    }
    return {
        'signalingState': this.pc_.signalingState,
        'iceGatheringState': this.pc_.iceGatheringState,
        'iceConnectionState': this.pc_.iceConnectionState
    };
}

getPeerConnectionStats (callback) {
    if (!this.pc_) {
        return;
    }
    this.pc_.getStats(null)
        .then(callback);
}

close() {
    if ( this.localStreams_) {
        this.localStreams_.getTracks().forEach(track => track.stop());
        this.localStreams_ = null;
    }

    if (!this.pc_) {
        return;
    }

    this.pc_.close();
    window.dispatchEvent(new CustomEvent('pcclosed', {
        detail: {
            pc: this,
            time: new Date(),
        }
    }));
    this.pc_ = null;
}

onRemoteStreamAdded_ (event) {
    if (!this.pc_) {
        return;
    }
    console.log('Remote stream added:' + event.streams );
    if( this.onRemoteVideoAdded )
        this.onRemoteVideoAdded( event.streams );
}

setLocalSdpAndSendAnswer_ (sdp) {
    this.pc_.setLocalDescription(sdp)
        .then(console.log('Set local session description success.'))
        .catch(this.onError_.bind(this, 'setLocalDescription'));

    if( this.sendSignalingMessage )  {
        console.log('Sending Answer: ', JSON.stringify(sdp));
        this.sendSignalingMessage(sdp);
    }
}

receivedSignalingMessage (message) {
    console.log('Received signaling message type :' + message['type'] );
    if (message['type'] === 'offer' ) {
        // Processing offer
        console.log('Offer from PeerConnection');
        this.pc_.setRemoteDescription(new RTCSessionDescription(message))
            .then(console.log('Set session description success.'))
            .catch(this.onError_.bind(this, 'setRemoteDescription'));

        console.log('Creating answer SDP to send peer.');
        this.pc_.createAnswer()
            .then(this.setLocalSdpAndSendAnswer_.bind(this))
            .catch(this.onError_.bind(this, 'createAnswer'));
        return;
    }
    else if (message['type'] === 'candidate' ) {
        // Processing candidate
        let ice_candidate = new RTCIceCandidate({
            sdpMLineIndex: message.label,
            sdpMid: message.id,
            candidate: message.candidate
        });

        this.pc_.addIceCandidate(ice_candidate)
            .then(() => {
                console.log('Remote candidate added successfully.');
            }).catch((error) => {
                console.error(`Failed to add Ice Candidate: ${error.toString()}`);
            });
        return;
    }
    console.error('Unknown signaling message: ', message );
}

onError_ (tag, error) {
    console.error(tag + ': ' + error.toString());
}


};  // class PeerConnectionClient

export default PeerConnectionClient;


