
# WebRTC streamer for Raspberry PI

## General
Notice:  This is a work in progress, 

This repo's objective is providing something like Web Cam server on the most popular Raspberry PI hardware. By integrating  [WebRTC](https://webrtc.org/native-code/) and Raspberry PI, we can stream the Raspberry camera feed to browser or native client which talks WebRTC.


#### H.264 Codec in Rpi Camera
H.264 codec supporting started at IEFT in late November 2014. Video Codec VP8 and H.264 both mandatory to implement in browser and WebRTC client. 

In the Raspberry PI, Video Codec does not give a lot of choice. in fact,  H.264 is the only option.
The Raspberry camera module can record video as H.264, baseline, main and high-profile formats.   ([Raspberry PI FAQ](https://www.raspberrypi.org/help/faqs/#topCamera))


#### Streaming camera feed
To get the camera feed from Raspberry PI, i.e. H.264 video stream, RWS(rpi-webrtc-streamer) use MMAL(Multi-Media Abstraction Layer) library can be found on [ARM side libraries for interfacing to Raspberry Pi GPU](https://github.com/raspberrypi/userland). it provides lower level API to multi-media components running on Broadcom VideoCore. For  convenience,  this streamer directly integrated  [raspivid](https://github.com/raspberrypi/userland/tree/master/host_applications/linux/apps/raspicam)
with encoding parameter changing in H.264 stream and passing video frame through WebRTC native code package.

####  Demo Video
Demo Video

<a href="http://www.youtube.com/watch?feature=player_embedded&v=I1E8MrA5lhw" target="_blank"><img src="http://img.youtube.com/vi/I1E8MrA5lhw/0.jpg" 
alt="" width="560" height="315" border="10" /></a>

####  WebRTC H.264 Codec supported browsers


Browser|Supported|H.264 Codec Status|
----------------|---------------|-----------|
Chrome |Yes|[Software encoder/decoder](https://www.chromestatus.com/feature/6417796455989248)|
Firefox|Yes|Browser plugin|
*it can be checked via the [browser webrtc supporting score](http://iswebrtcreadyyet.com/)*

## Hardware Requirement
TBD


## Cross Compile on Ubuntu Linux
#### Cross Compiler setup	
Please refer to [BUILD_ubuntu.md document](../master/BUILD_ubuntu.md).

## Running on Raspberry PI
Please refer to [CONFIGURE_rws.md document](../master/CONFIGURE_rws.md).

## Known Issues and Bugs
TBD

## TODO
TBD

## Version
 * 2016/09/20 v0.56 : Initial Version


