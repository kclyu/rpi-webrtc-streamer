
'use strict';

var connectButton = document.getElementById("Connect")
var disconnectButton = document.getElementById("Disconnect")
var remoteVideoElement = document.getElementById("remoteVideo")

var websocketSignalingChannel = 
    new WebSocketSignalingChannel(connectButton, disconnectButton, remoteVideoElement);




