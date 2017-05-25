
'use strict';

var connectButton = document.getElementById("Connect")
var disconnectButton = document.getElementById("Disconnect")
var minBitrateSelector = document.querySelector('select#minBitrate');
var maxBitrateSelector = document.querySelector('select#maxBitrate');

function disconnectButtonState() {
    connectButton.disabled = false;
    disconnectButton.disabled = true;
    minBitrateSelector.disabled = false;
    maxBitrateSelector.disabled = false;
}

function connectButtonState() {
    connectButton.disabled = true;
    disconnectButton.disabled = false;
    minBitrateSelector.disabled = true;
    maxBitrateSelector.disabled = true;
}

function notSupportedButtonState() {
    connectButton.disabled = true;
    disconnectButton.disabled = true;
    minBitrateSelector.disabled = true;
    maxBitrateSelector.disabled = true;
}

connectButton.onclick = doConnect;
disconnectButton.onclick = doDisconnect;

disconnectButtonState();

// Graph
var bitrateGraph;
var bitrateSeries;

var packetGraph;
var packetSeries;

var lastPacketsReceived;
var lastBytesReceived;
var lastTimeStamp;

function isPrivateIP(ip) {
    var parts = ip.split('.');
    return parts[0] === '10' || 
        (parts[0] === '172' && (parseInt(parts[1], 10) >= 16 && parseInt(parts[1], 10) <= 31)) || 
        (parts[0] === '192' && parts[1] === '168');
}

window.onbeforeunload = doDisconnect;

var messageCounter = 0;
var peerConnection;    

var localTestingUrl = "ws://10.0.0.11:8889/rws/ws";
var pcConfig = {"iceServers": [{"url": "stun:stun.l.google.com:19302"}]};
var pcOptions = { optional: [ {DtlsSrtpKeyAgreement: true} ] };
var mediaConstraints = {'mandatory': { 'OfferToReceiveAudio': true, 'OfferToReceiveVideo': true }};
var remoteStream;

function onIceCandidate(pc, event) {
  getOtherPc(pc).addIceCandidate(event.candidate)
  .then(
    function() {
      onAddIceCandidateSuccess(pc);
    },
    function(err) {
      onAddIceCandidateError(pc, err);
    }
  );
  trace(getName(pc) + ' ICE candidate: \n' + (event.candidate ?
      event.candidate.candidate : '(null)'));
}

function onAddIceCandidateSuccess() {
  trace('AddIceCandidate success.');
}

function onAddIceCandidateError(error) {
  trace('Failed to add ICE Candidate: ' + error.toString());
}

function onSetSessionDescriptionError(error) {
  trace('Failed to set session description: ' + error.toString());
}

///////////////////////////////////////////////////////////////////////////////
//
// PeerConnection 
//
///////////////////////////////////////////////////////////////////////////////
function createPeerConnection() {
	try {
		peerConnection = new RTCPeerConnection(pcConfig, pcOptions);
		peerConnection.onicecandidate = function(event) {
			if (event.candidate) {
				var candidate = { type: 'candidate',
					label: event.candidate.sdpMLineIndex,
					id: event.candidate.sdpMid,
					candidate: event.candidate.candidate
				};
				doSend(JSON.stringify(candidate));
			} else {
				trace("End of candidates.");
			}
		};
		peerConnection.onconnecting = onSessionConnecting;
		peerConnection.onopen = onSessionOpened;
		peerConnection.onaddstream = onRemoteStreamAdded;
		peerConnection.onremovestream = onRemoteStreamRemoved;
		trace("Created RTCPeerConnnection with config: " + JSON.stringify(pcConfig));
	} 
	catch (e) {
		trace("Failed to create PeerConnection with " + 
                connectionId + ", exception: " + e.message);
	}

    bitrateSeries = new TimelineDataSeries();
    bitrateGraph = new TimelineGraphView('bitrateGraph', 'bitrateCanvas');
    bitrateGraph.updateEndDate();

    packetSeries = new TimelineDataSeries();
    packetGraph = new TimelineGraphView('packetGraph', 'packetCanvas');
    packetGraph.updateEndDate();
}

function onRemoteStreamAdded(event) {
    trace("Remote stream added:", event.stream );
    var remoteVideoElement = document.getElementById('remoteVideo');
    remoteVideo.srcObject = event.stream;
}

function sld_success_cb() {
	trace("setLocalDescription success");
}

function sld_failure_cb() {
	trace("setLocalDescription failed");
}

function aic_success_cb() {
	trace("addIceCandidate success");
}

function aic_failure_cb() {
	trace("addIceCandidate failed");
}

function doHandlePeerMessage(data) {
    ++messageCounter;
    var minBitrate = minBitrateSelector.options[minBitrateSelector.selectedIndex].value;
    var maxBitrate = maxBitrateSelector.options[maxBitrateSelector.selectedIndex].value;
    var dataJson = JSON.parse(data);
    trace("Handle Message :", JSON.stringify(dataJson));

    if (dataJson["type"] == "offer" ) {        // Processing offer
        trace("Offer from PeerConnection" );
        var sdp_returned = forceChosenVideoCodec(dataJson.sdp, 'H264/90000');

        trace("SDP RETURN : " + sdp_returned);
        dataJson.sdp = sdp_returned;

        // Creating PeerConnection
        createPeerConnection();
        peerConnection.setRemoteDescription(new RTCSessionDescription(dataJson), onRemoteSdpSucces, onRemoteSdpError);              
        peerConnection.createAnswer(function(sessionDescription) {

            trace("Create answer:", sessionDescription);
            peerConnection.setLocalDescription(sessionDescription,sld_success_cb,sld_failure_cb);

            trace("Bitrate Setting Min:" + minBitrate + ", Max: " + maxBitrate );
            sessionDescription.sdp = setVideoInitialBitRate(sessionDescription.sdp,'H264/90000', minBitrate, maxBitrate);
            var data = JSON.stringify(sessionDescription);
            trace("Sending Answer: " + data );
            doSend(data);
        }, function(error) { // error
            trace("Create answer error:", error);
        }, mediaConstraints); // type error           
    }
    else if (dataJson["type"] == "candidate" ) {    // Processing candidate
        trace("Adding ICE candiate " + dataJson["candidate"]);
        var candidate = new RTCIceCandidate({sdpMLineIndex: dataJson.sdpMLineIndex, candidate: dataJson.candidate});
        peerConnection.addIceCandidate(candidate, aic_success_cb, aic_failure_cb);
    }
}

function onSessionConnecting(message) {
	trace("Session connecting.");
}

function onSessionOpened(message) {
	trace("Session opened.");
}

function onRemoteStreamRemoved(event) {
	trace("Remote stream removed.");
}

function onRemoteSdpError(event) {
	console.error('onRemoteSdpError', event.name, event.message);
}

function onRemoteSdpSucces() {
	trace('onRemoteSdpSucces');
} 

// query getStats every second
window.setInterval(function() {
    if (!window.peerConnection) {
        return;
    }
    window.peerConnection.getStats(function(response) {
        var bytes;
        var packets;
        var now;
        response.result().forEach(function(report) {
            // var standardStats = {
            //     id: report.id,
            //    type: report.type
            //};
            // console.log(report);
            if(report.type === 'ssrc' && report.stat('mediaType') === 'video') {
                bytes  = report.stat('bytesReceived');
                packets  = report.stat('packetsReceived');
                now  = report.timestamp;
                if (lastPacketsReceived != 0) {
                    // calculate bitrate
                    var bitrate = 8 * (bytes - lastBytesReceived) /
                        (now - lastTimeStamp);

                    // append to chart
                    bitrateSeries.addPoint(now, bitrate);
                    bitrateGraph.setDataSeries([bitrateSeries]);
                    bitrateGraph.updateEndDate();

                    // calculate number of packets and append to chart
                    packetSeries.addPoint(now, packets -
                            lastPacketsReceived);
                    packetGraph.setDataSeries([packetSeries]);
                    packetGraph.updateEndDate();
                }
                lastPacketsReceived = packets;
                lastBytesReceived = bytes;
                lastTimeStamp = now;
            };
        });
    });
}, 1000);


///////////////////////////////////////////////////////////////////////////////
//
// WebSocket 
//
///////////////////////////////////////////////////////////////////////////////
var websocket;
function ConnectWebSocket() {
    var websocket_url;
    if( location.hostname == "127.0.0.1" ) 
        websocket_url = localTestingUrl;
    else if( isPrivateIP( location.host ) )
        websocket_url = "ws://" + location.host + "/rws/ws";
    else
        websocket_url = "wss://" + location.hostname + "/rws/ws";
    websocket = new WebSocket(websocket_url);
    websocket.onopen = function(event) { onOpen(event) };
    websocket.onclose = function(event) { onClose(event) };
    websocket.onmessage = function(event) { onMessage(event) };
    websocket.onerror = function(event) { onError(event) };
}

function onOpen(event) {
    trace("Websocket connnected: " + websocket.url);
    doRegister();
}

function onClose(event) {
    trace("Websocket Disconnected");
    doDisconnect();
}

function onMessage(event) {
    trace("WSS -> C: " + event.data);

    var dataJson = JSON.parse(event.data);
    if( dataJson["cmd"] == "send") {
        doHandlePeerMessage(dataJson["msg"]);
    }
}

function onError(event) {
    trace("An error occured while connecting : " + event.data);
}

// window.addEventListener("load", init, false);

function WebSocketSend(message) {
    if( websocket.readyState == WebSocket.OPEN ||
        websocket.readyState == WebSocket.CONNECTING)  {
        trace("C --> WSS: " + message);
        websocket.send(message);
        // var data = new ArrayBuffer(message);
        //var byteArray = new Uint8Array(message);
        //websocket.send(byteArray.buffer);
        return true;
    }
    trace("failed to send :" + message);
    return false;
}


// Return a random numerical string.
function randomString(strLength) {
    var result = [];
    strLength = strLength || 5;
    var charSet = '0123456789';
    while (strLength--) {
        result.push(charSet.charAt(Math.floor(Math.random() * charSet.length)));
    }
    return result.join('');
}



function doConnect() {
    // check whether WebSocket is suppored in this browser.
    if( window.WebSocket ){
        // supported
        connectButtonState();
        ConnectWebSocket();
    }else{
		trace("doConnect: WebSocket is not suppported in this platform!");
        // WebSocket is not supported
        notSupportedButtonState();
    }
}

function doRegister () {
    // No Room concept, random generate room and client id.
    var register = { cmd: 'register',
        roomid: randomString(9),
        clientid: randomString(8)
    };
    var register_message = JSON.stringify(register);
    WebSocketSend(register_message);
}

function doSend (data) {
    var message = { cmd: "send",
        msg: data,
        error: ""
    };
    var data_message = JSON.stringify(message);
    if( WebSocketSend(data_message) == false ) {
		trace("Failed to send data: " + data_message);
        return false;
    };
    return true;
}

function doDisconnect() {
    disconnectButtonState();
    if( websocket.readyState == 1)  {
        websocket.close();
    };

    peerConnection = null;    
    lastPacketsReceived = 0;
    lastBytesReceived = 0;
    lastTimeStamp = 0;
}




