
# WebRTC streamer for Raspberry PI

## General
Notice:  This is a work in progress, 

This repo's objective is providing something like Web Cam server on the most popular Raspberry PI hardware. By integrating  [WebRTC](https://webrtc.org/native-code/) and Raspberry PI, we can stream the Raspberry camera feed to browser or native client which talks WebRTC.

Generally, the components of WebRTC service are classified into Signaling Server and WebRTC client. However, RWS(Rpi-WebRTC-Streamer) is built to operate on one piece of Raspberry PI hardware and includes some of Signaling Server functionality. In other words, the Browser or Client supporting WebRTC directly connects to RWS and receives WebRTC streaming service.

### Streaming camera feed
To get the camera feed from Raspberry PI, i.e. H.264 video stream, RWS use MMAL(Multi-Media Abstraction Layer) library can be found on [ARM side libraries for interfacing to Raspberry Pi GPU](https://github.com/raspberrypi/userland). it provides lower level API to multi-media components running on Broadcom VideoCore. For  convenience,  this streamer directly integrated  [raspivid](https://github.com/raspberrypi/userland/tree/master/host_applications/linux/apps/raspicam)
with encoding parameter changing in H.264 stream and passing video frame through WebRTC native code package.

### Motion Detection
Motion Detection feature provided by Rpi-WebRTC-Streamer uses Inline Motion Vector which is generated during video encoding.  And use this to get the approximate Motion Detection function while using minimal CPU resources.

Please refer to [README_motion document](../master/README_motion.md).

### Messenger Notification 
The motion detection event message can be transmitted to the user through the telegram messenger. It can send motion detection message and detected video clip to Telegram Messenger client so that it can be used as a private security bot.
For more information, Please refer to [README_TelegramBot document](../master/README_TelegramBot.md).


## Rpi WebRTC Streamer
### Demo Video

<a href="http://www.youtube.com/watch?feature=player_embedded&v=I1E8MrA5lhw" target="_blank"><img src="http://img.youtube.com/vi/I1E8MrA5lhw/0.jpg" 
alt="" width="560" height="315" border="10" /></a>

###  WebRTC H.264 Codec supported browsers

|Browser|Supported|H.264 Codec Status|
|----------------|---------------|-----------|
|Chrome |Yes|Software encoder/decoder|
|Opera |Yes|Software encoder/decoder|
|Firefox|Yes|Browser plugin|

## Hardware Requirement
### Raspberry PI 
- Raspberry PI 2/3
- Raspberry Pi Zero/Zero W (ZeroW tested)

### Video Camera
- RPI Camera board V1/V2
- Arducam 5 Megapixels 1080p Sensor OV5647 Mini Camera Video Module

### Audio hardware
Please refer to the  [README_audio.md](https://github.com/kclyu/rpi-webrtc-streamer/blob/master/README_audio.md)

## Running RWS on Raspberry PI
Please refer to [README_rws_setup.md document](../master/README_rws_setup.md).

## Download Deb package for Testing
To download RWS deb package, please refer to the following URL. RWS is currently in development and testing, so please use it with consideration.

Please refer to [Rpi-Webrtc-Streamer-deb Repo](https://github.com/kclyu/rpi-webrtc-streamer-deb).

## Cross Compile on Ubuntu Linux

Please refer to [README_building.md document](../master/README_building.md).

## Known Issues and Bugs
TBD

## TODO
TBD

## Version History
* 2018/01/13 v0.73:
   - Adding Telegram Bot to convert detected motion video file to MP4 and 
     sends message to Telegram Messenger client
   - Motion detection video syncing with 
* 2017/11/29 v0.72:
	- Motion detection using IMV(Inline Motion Vector)
	- Motion detected part saved as video file
* 2017/06/09 v0.65:
	- Adding log file shifting between folders
    - Prepare first deb package for systemd	
* 2017/04/28 v0.62 : 
     - Bandwith base Quality Control is added
     - Video Resolution ratio 16: 9, 4: 3 config added
        - 4:3:  320:240 ~ 1296:972
        - 16:9  384:216 ~ 1408:864 
* 2017/04/04 v0.60 : 
     - signaling interface changed from long-poll to websocket (libwebsockets)
     - RWS main port changed from 8888 to 8889 (because Android Direct socket need 8888 port)
 * 2017/01/10 v0.57 : 
     - adding initial android direct socket feature
     - fixing branch-heads/55
     - removing unused GYP building scripts and files
     - webrtc build directory changed from 'arm/out/Debug' to 'arm_build'
 * 2016/09/20 v0.56 : Initial Version

 

