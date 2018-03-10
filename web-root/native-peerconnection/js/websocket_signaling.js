
'use strict';

///////////////////////////////////////////////////////////////////////////////
//
// WebSocket Signaling Channel
//
///////////////////////////////////////////////////////////////////////////////

function WebSocketSignalingChannel(connectButton,disconnectButton,remoteVideo, wsPath ) {
    this.connectButton_ = connectButton;
    this.disconnectButton_ = disconnectButton;
    this.remoteVideo_ = remoteVideo;
    this.wsPath_ = wsPath;

    this.connectButton_.disabled = false;
    this.disconnectButton_.disabled = true;
    this.connectButton_.onclick = this.doSignalingConnect.bind(this);
    this.disconnectButton_.onclick = this.doSignalingDisconnnect.bind(this);
    window.onbeforeunload = this.doSignalingDisconnnect.bind(this);
};


WebSocketSignalingChannel.prototype.connectWebSocket = function () {
    var websocket_url;
    var websocket_url_path = this.wsPath_ || "/rws/ws";
    if( this.isPrivateIPaddress_( location.host ) )
        websocket_url = "ws://" + location.host + websocket_url_path;
    else
        websocket_url = "wss://" + location.host + websocket_url_path;
    trace("WebSocket URL : " + websocket_url);

    this.websocket_ = new WebSocket(websocket_url);
    this.websocket_.onopen =  this.onWebSocketOpen_.bind(this);
    this.websocket_.onclose = this.onWebSocketClose_.bind(this);
    this.websocket_.onmessage = this.onWebSocketMessage_.bind(this);
    this.websocket_.onerror = this.onWebSocketError_.bind(this);
};

WebSocketSignalingChannel.prototype.onWebSocketOpen_ = function (event) {
    trace("Websocket connnected: " + this.websocket_.url);
    this.doSignalingRegister();
};

WebSocketSignalingChannel.prototype.onWebSocketClose_ = function (event) {
    trace("Websocket Disconnected");
    this.doSignalingDisconnnect();
    this.peerConnectionClient_.close();
    this.peerConnectionClient_ = null;
};

WebSocketSignalingChannel.prototype.onWebSocketMessage_ = function (event) {
    trace("WSS -> C: " + event.data);

    var dataJson = JSON.parse(event.data);
    if( dataJson["cmd"] == "send") {
        this.peerConnectionClient_.onReceivePeerMessage(dataJson["msg"]);
    }
};

WebSocketSignalingChannel.prototype.onWebSocketError_ = function (event) {
    trace("An error occured while connecting : " + event.data);
    // TODO: need error handling
};

WebSocketSignalingChannel.prototype.WebSocketSendMessage = function (message) {
    if( this.websocket_.readyState == WebSocket.OPEN ||
        this.websocket_.readyState == WebSocket.CONNECTING)  {
        trace("C --> WSS: " + message);
        this.websocket_.send(message);
        return true;
    }
    trace("failed to send websocket message :" + message);
    return new Error('failed to send websocket message');
};


WebSocketSignalingChannel.prototype.doSignalingConnect = function () {
    // check whether WebSocket is suppored in this browser.
    if( window.WebSocket ){
        // supported
        this.connectButton_.disabled = true;
        this.disconnectButton_.disabled = false;
        this.peerConnectionClient_ = 
            new PeerConnectionClient(this.remoteVideo_, this.doSignalingSend.bind(this) );
        this.connectWebSocket();
    }else{
		trace("doSignalingConnect: WebSocket is not suppported in this platform!");
        // WebSocket is not supported
        this.connectButton_.disabled = true;
        this.disconnectButton_.disabled = true;
    }
};

// 
WebSocketSignalingChannel.prototype.doSignalingRegister = function () {
    // No Room concept, random generate room and client id.
    var register = { cmd: 'register',
        roomid: this.randomString_(9),
        clientid: this.randomString_(8)
    };
    var register_message = JSON.stringify(register);
    this.WebSocketSendMessage(register_message);
};

WebSocketSignalingChannel.prototype.doSignalingSend = function (data) {
    var message = { cmd: "send",
        msg: JSON.stringify(data),
        error: ""
    };
    var data_message = JSON.stringify(message);
    if( this.WebSocketSendMessage(data_message) == false ) {
		trace("Failed to send data: " + data_message);
        return false;
    };
    return true;
};

WebSocketSignalingChannel.prototype.doSignalingDisconnnect = function () {
    this.connectButton_.disabled = false;
    this.disconnectButton_.disabled = true;
    if( this.websocket_.readyState == 1)  {
        this.websocket_.close();
    };
};

///////////////////////////////////////////////////////////////////////////////
//
// Utility Helper functions
//
///////////////////////////////////////////////////////////////////////////////

WebSocketSignalingChannel.prototype.isPrivateIPaddress_ = function (ipaddress) {
    var parts = ipaddress.split('.');
    return parts[0] === '10' || 
        (parts[0] === '172' && (parseInt(parts[1], 10) >= 16 && parseInt(parts[1], 10) <= 31)) || 
        (parts[0] === '192' && parts[1] === '168');
};

WebSocketSignalingChannel.prototype.randomString_ = function (length) {
// Return a random numerical string.
    var result = [];
    var strLength = length || 5;
    var charSet = '0123456789';
    while (strLength--) {
        result.push(charSet.charAt(Math.floor(Math.random() * charSet.length)));
    }
    return result.join('');
};


