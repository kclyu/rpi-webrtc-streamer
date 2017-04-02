
'use strict';

var connectButton = document.getElementById("Connect")
var disconnectButton = document.getElementById("Disconnect")
connectButton.disabled = false;
disconnectButton.disabled = true;
connectButton.onclick = doConnect;
disconnectButton.onclick = doDisconnect;

window.onbeforeunload = doDisconnect;

var messageCounter = 0;
var peerConnection;    

var localTestingUrl = "ws://10.0.0.11:8888/rws/ws";
var pcConfig = {"iceServers": [{"url": "stun:stun.l.google.com:19302"}]};
var pcOptions = { optional: [ {DtlsSrtpKeyAgreement: true} ] };
var mediaConstraints = {'mandatory': { 'OfferToReceiveAudio': true, 'OfferToReceiveVideo': true }};
var remoteStream;


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
    var dataJson = JSON.parse(data);
    trace("Handle Message :", JSON.stringify(dataJson));

    if (dataJson["type"] == "offer" ) {        // Processing offer
        trace("Offer from PeerConnection" );
        var sdp_returned = forceChosenVideoCodec(dataJson.sdp, 'H264/90000');
        dataJson.sdp = sdp_returned;
        // Creating PeerConnection
        createPeerConnection();
        peerConnection.setRemoteDescription(new RTCSessionDescription(dataJson), onRemoteSdpSucces, onRemoteSdpError);              
        peerConnection.createAnswer(function(sessionDescription) {
            trace("Create answer:", sessionDescription);
            peerConnection.setLocalDescription(sessionDescription,sld_success_cb,sld_failure_cb);
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


function forceChosenVideoCodec(sdp, codec) {
	return maybePreferCodec(sdp, 'video', 'send', codec);
}

function forceChosenAudioCodec(sdp, codec) {
	return maybePreferCodec(sdp, 'audio', 'send', codec);
}

// Copied from AppRTC's sdputils.js:

// Sets |codec| as the default |type| codec if it's present.
// The format of |codec| is 'NAME/RATE', e.g. 'opus/48000'.
function maybePreferCodec(sdp, type, dir, codec) {
	var str = type + ' ' + dir + ' codec';
	if (codec === '') {
		trace('No preference on ' + str + '.');
		return sdp;
	}

	trace('Prefer ' + str + ': ' + codec);	// kclyu

	var sdpLines = sdp.split('\r\n');

	// Search for m line.
	var mLineIndex = findLine(sdpLines, 'm=', type);
	if (mLineIndex === null) {
		trace('* not found error: ' + str + ': ' + codec );	// kclyu
		return sdp;
	}

	// If the codec is available, set it as the default in m line.
	var codecIndex = findLine(sdpLines, 'a=rtpmap', codec);
	trace('mLineIndex Line: ' +  sdpLines[mLineIndex] );
	trace('found Prefered Codec in : ' + codecIndex + ': ' + sdpLines[codecIndex] );
	trace('codecIndex', codecIndex);
	if (codecIndex) {
		var payload = getCodecPayloadType(sdpLines[codecIndex]);
		if (payload) {
			sdpLines[mLineIndex] = setDefaultCodec(sdpLines[mLineIndex], payload);
			//sdpLines[mLineIndex] = setDefaultCodecAndRemoveOthers(sdpLines, sdpLines[mLineIndex], payload);
		}
	}

	// delete id 100(VP8) and 101(VP8)

	trace('** Modified LineIndex Line: ' +  sdpLines[mLineIndex] );
	sdp = sdpLines.join('\r\n');
	return sdp;
}

// Find the line in sdpLines that starts with |prefix|, and, if specified,
// contains |substr| (case-insensitive search).
function findLine(sdpLines, prefix, substr) {
	return findLineInRange(sdpLines, 0, -1, prefix, substr);
}

// Find the line in sdpLines[startLine...endLine - 1] that starts with |prefix|
// and, if specified, contains |substr| (case-insensitive search).
function findLineInRange(sdpLines, startLine, endLine, prefix, substr) {
	var realEndLine = endLine !== -1 ? endLine : sdpLines.length;
	for (var i = startLine; i < realEndLine; ++i) {
		if (sdpLines[i].indexOf(prefix) === 0) {
			if (!substr ||
					sdpLines[i].toLowerCase().indexOf(substr.toLowerCase()) !== -1) {
				return i;
			}
		}
	}
	return null;
}

// Gets the codec payload type from an a=rtpmap:X line.
function getCodecPayloadType(sdpLine) {
	var pattern = new RegExp('a=rtpmap:(\\d+) \\w+\\/\\d+');
	var result = sdpLine.match(pattern);
	return (result && result.length === 2) ? result[1] : null;
}

// Returns a new m= line with the specified codec as the first one.
function setDefaultCodec(mLine, payload) {
	var elements = mLine.split(' ');

	// Just copy the first three parameters; codec order starts on fourth.
	var newLine = elements.slice(0, 3);

	// Put target payload first and copy in the rest.
	newLine.push(payload);
	for (var i = 3; i < elements.length; i++) {
		if (elements[i] !== payload) {
			newLine.push(elements[i]);
		}
	}
	return newLine.join(' ');
}


function setDefaultCodecAndRemoveOthers(sdpLines, mLine, payload) {
	var elements = mLine.split(' ');

	// Just copy the first three parameters; codec order starts on fourth.
	var newLine = elements.slice(0, 3);
	

	// Put target payload first and copy in the rest.
	newLine.push(payload);
	for (var i = 3; i < elements.length; i++) {
		if (elements[i] !== payload) {

			//  continue to remove all matching lines
			for(var line_removed = true;line_removed;) {
				line_removed = RemoveLineInRange(sdpLines, 0, -1, "a=rtpmap", elements[i] );
			}
			//  continue to remove all matching lines
			for(var line_removed = true;line_removed;) {
				line_removed = RemoveLineInRange(sdpLines, 0, -1, "a=rtcp-fb", elements[i] );
			}
		}
	}
	return newLine.join(' ');
}

function RemoveLineInRange(sdpLines, startLine, endLine, prefix, substr) {
	var realEndLine = endLine !== -1 ? endLine : sdpLines.length;
	for (var i = startLine; i < realEndLine; ++i) {
		if (sdpLines[i].indexOf(prefix) === 0) {
			if (!substr ||
					sdpLines[i].toLowerCase().indexOf(substr.toLowerCase()) !== -1) {
				var str = "Deleting(index: " + i + ") : " + sdpLines[i];
				trace(str);
				sdpLines.splice(i, 1);
				return true;
			}
		}
	}
	return false;
}

// query getStats every second
window.setInterval(function() {
		if (!window.pc1) {
		return;
		}
		window.pc1.getStats(null).then(function(res) {
			Object.keys(res).forEach(function(key) {
				var report = res[key];
				var bytes;
				var packets;
				var now = report.timestamp;
				if ((report.type === 'outboundrtp') ||
					(report.type === 'outbound-rtp') ||
					(report.type === 'ssrc' && report.bytesSent)) {
				bytes = report.bytesSent;
				packets = report.packetsSent;
				if (lastResult && lastResult[report.id]) {
				// calculate bitrate
				var bitrate = 8 * (bytes - lastResult[report.id].bytesSent) /
				(now - lastResult[report.id].timestamp);

				// append to chart
				bitrateSeries.addPoint(now, bitrate);
				bitrateGraph.setDataSeries([bitrateSeries]);
				bitrateGraph.updateEndDate();

				// calculate number of packets and append to chart
				packetSeries.addPoint(now, packets -
						lastResult[report.id].packetsSent);
				packetGraph.setDataSeries([packetSeries]);
				packetGraph.updateEndDate();
				}
				}
			});
			lastResult = res;
		});
}, 1000);

///////////////////////////////////////////////////////////////////////////////
//
// WebSocket 
//
///////////////////////////////////////////////////////////////////////////////
// TODO refactoring is reqquired
var websocket;
function ConnectWebSocket() {
    var websocket_url;
    if( location.hostname == "127.0.0.1" ) 
        websocket_url = localTestingUrl;
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
        connectButton.disabled = true;
        disconnectButton.disabled = false;
        ConnectWebSocket();
    }else{
		trace("doConnect: WebSocket is not suppported in this platform!");
        // WebSocket is not supported
        connectButton.disabled = true;
        disconnectButton.disabled = true;
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
    connectButton.disabled = false;
    disconnectButton.disabled = true;
    if( websocket.readyState == 1)  {
        websocket.close();
    };
}


