
'use strict';

///////////////////////////////////////////////////////////////////////////////
//
// PeerConnection 
//
// The PeerConnectionClient in the code below is coded with reference 
// to the required part of the apprtc peerconnectionclient.js and 
// the configuration flow and method of the PeeConnection Client.
//
///////////////////////////////////////////////////////////////////////////////
//
// TODO:
//  - need to add PC configuration params before PC creation
//  - need to consider peer connection drop error handing 
//  - need to add new inteface to configure for rws config parameters
//
////////////////////////////////////////////////////////////////////////////////

function PeerConnectionClient(remoteVideo, doSendPeerMessage) {
    // configuration for peerconnection
    this.pcConfig_ = {"iceServers": [{"urls": "stun:stun.l.google.com:19302"}]};
    this.pcOptions_ = { optional: [ {DtlsSrtpKeyAgreement: true} ] };

    this.messageCounter_ = 0;
    this.doSendPeerMessage_ = doSendPeerMessage;
    this.remoteVideo_ = remoteVideo;

    //  PeerConnection
    this.peerConnection_ = new RTCPeerConnection(this.pcConfig_, this.pcOptions_);
    this.peerConnection_.onicecandidate = this.onIceCandidate_.bind(this);
    this.peerConnection_.ontrack = this.onRemoteStreamAdded_.bind(this);
    this.peerConnection_.onremovestream = trace.bind(null, 'Remote stream removed.');
    this.peerConnection_.onsignalingstatechange = this.onSignalingStateChanged_.bind(this);
    this.peerConnection_.oniceconnectionstatechange =
        this.onIceConnectionStateChanged_.bind(this);
    trace("Created RTCPeerConnnection with config: " + JSON.stringify(this.pcConfig_));
};

PeerConnectionClient.prototype.onIceCandidate_ = function(event) {
    if (!this.peerConnection_) {
        return;
    }
    if (event.candidate) {
        var message = {
            type: 'candidate',
            label: event.candidate.sdpMLineIndex,
            id: event.candidate.sdpMid,
            candidate: event.candidate.candidate
        };
        trace('Sending IceCandidate : ' + JSON.stringify(message));
        this.doSendPeerMessage_(message);
    } else {
        trace('End of candidates.');
    }
};

PeerConnectionClient.prototype.onIceConnectionStateChanged_ = function() {
    if (!this.peerConnection_) {
        return;
    }
    trace('ICE connection state changed to: ' + this.peerConnection_.iceConnectionState);
    if (this.peerConnection_.iceConnectionState === 'completed') {
        trace('ICE complete time: ' +
                (window.performance.now() - this.startTime_).toFixed(0) + 'ms.');
    }
};

PeerConnectionClient.prototype.onSignalingStateChanged_ = function() {
    if (!this.peerConnection_) {
        return;
    }
    trace('Signaling state changed to: ' + this.peerConnection_.signalingState);
};

PeerConnectionClient.prototype.getPeerConnectionStates = function() {
    if (!this.peerConnection_) {
        return null;
    }
    return {
        'signalingState': this.peerConnection_.signalingState,
        'iceGatheringState': this.peerConnection_.iceGatheringState,
        'iceConnectionState': this.peerConnection_.iceConnectionState
    };
};

PeerConnectionClient.prototype.getPeerConnectionStats = function(callback) {
    if (!this.peerConnection_) {
        return;
    }
    this.peerConnection_.getStats(null)
        .then(callback);
};


PeerConnectionClient.prototype.close = function() {
    if (!this.peerConnection_) {
        return;
    }

    this.peerConnection_.close();
    window.dispatchEvent(new CustomEvent('peerconnectionclosed', {
        detail: {
            pc: this,
            time: new Date(),
        }
    }));
    this.peerConnection_ = null;
};


PeerConnectionClient.prototype.onRemoteStreamAdded_ = function (event) {
    if (!this.peerConnection_) {
        return;
    }
    if( !this.remoteVideo_ ) {
        trace("Error in Remote Video Element object :" );
        return;
    }
    trace("Remote stream added:" + event.streams );
    this.remoteVideo_.srcObject = event.streams[0];
};

PeerConnectionClient.prototype.setLocalSdpAndSend_ = function(sessionDescription) {
    this.peerConnection_.setLocalDescription(sessionDescription)
        .then(trace.bind(null, 'Set local session description success.'))
        .catch(this.onError_.bind(this, 'setLocalDescription'));

    this.doSendPeerMessage_(sessionDescription) ;
};

PeerConnectionClient.prototype.onReceivePeerMessage = function (data) {
    ++this.messageCounter_;
    var dataJson = JSON.parse(data);
    trace("onReceived Message Type :" + dataJson['type'] );
    if (dataJson["type"] == "offer" ) {        // Processing offer
        trace("Offer from PeerConnection" );
        var sdp_returned = forceChosenVideoCodec(dataJson.sdp, 'H264/90000');
        dataJson.sdp = sdp_returned;

        this.peerConnection_.setRemoteDescription(new RTCSessionDescription(dataJson))
            .then(trace.bind(null, 'Set session description success.'))
            .catch(this.onError_.bind(this, 'setRemoteDescription'));

        trace('Sending answer to peer.');
        this.peerConnection_.createAnswer()
            .then(this.setLocalSdpAndSend_.bind(this))
            .catch(this.onError_.bind(this, 'createAnswer'));
    }
    else if (dataJson["type"] == "candidate" ) {    // Processing candidate
        var ice_candidate = new RTCIceCandidate({
            sdpMLineIndex: dataJson.label,
            sdpMid: dataJson.id,
            candidate: dataJson.candidate
        });
        this.peerConnection_.addIceCandidate(ice_candidate)
            .then(trace.bind(null, 'Remote candidate added successfully.'))
            .catch(this.onError_.bind(this, 'addIceCandidate'));
    }
};

PeerConnectionClient.prototype.onError_ = function(tag, error) {
    trace(tag + ': ' + error.toString());
};


