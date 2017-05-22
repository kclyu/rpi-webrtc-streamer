
# Raspberry PI Audio Setup for RWS

## General
In RWS, PulseAudio is disabled and uses ALSA. To use audio, use a USB audio dongle or USB microphone that has been confirmed to work with Raspberry PI. This document is based on the device whose operation has been verified and will be added if any device operation is confirmed.

## Want to buy Audio Device?
The Raspberry PI forum audio articles have a large number of posts , but there are not many latest updates of USB audio device for new hardware. One thing I would like to ask you first is to try to purchase a audio device that has been confirmed to be a hardware device that runs on Raspberry PI before purchasing a device for Audio Setup.

For example, USB 3.0 hub (power hub) or dongle does not work with Raspberry PI and some USB 2.0 dongle may or may not work.

See [USB Sound Cards](http://elinux.org/RPi_VerifiedPeripherals#USB_Sound_Cards), which is confirmed to work with Raspberry PI).

## Audio Device Setup
The linux audio part of the WebRTC native code package first searches all available audio device lists in the OS. However, the audio device you want to use should be set to search first.

#### 1. Disable raspberry audio
```
sudo vi /boot/config.txt
#dtparam=audio=on ## comment out
```
#### 2. Reorder USB audio device index 
```
sudo vi /lib/modprobe.d/aliases.conf
#options snd-usb-audio index=-2 # comment out
```
#### 3. change asoundrc
```
vi ~/.asoundrc
defaults.pcm.card 0;
defaults.ctl.card 0;
```


## USB audio dongle
An example of using USB audio dongle is [A presence robot with Chromium, WebRTC, Raspberry Pi 3 and EasyRTC](https://planb.nicecupoftea.org/2016/10/24/a-presence-robot-with-chromium-webrtc-raspberry-pi-3-and-easyrtc/)

## Working Audio Device List for WebRTC audio
- USB microphone - Cosy USB microphone MK1343UB

## Do you have any working configuration on Audio?
It is not easy to buy as many hardware as possible for the test, and there is not really much time to test it. If you have successfully set up your audio with your Raspberry PI and have verified audio transmission from RWS, please share it. I think it will help a lot of others.

Please add the comment to the issue as shown below.

- Device Type: (USB dongle | USB microphone | Raspberry PI audio hat | etc)
- Device Name: (Device Model)
- Command Output:
    - lsusb
	- cat / proc / asound / cards


## Testing Required

- Raspberry PI Audio Hat
- USB audio dongle


## Audio Setup - Version History

 * 2017/05/21 : Initial Version

 

