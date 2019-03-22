
# Building RPi-WebRTC-Streamer in Ubuntu

This Rpi-WebRTC-Streamer (RWS) builing document describes the procedure for cross compiling RWS in Ubuntu Linux.
For more information on tools or source repo, please refer to the linked URL and do not explain it separately.

## 1. Required Tools

###  1.1. [depot_tools](http://dev.chromium.org/developers/how-tos/depottools)

```
mkdir ~/tools
cd ~/tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:$HOME/tools/depot_tools
```
*You can use the directory named tools in the desired directory. Just add depot_tools to your path so that you can access it directly from the command line.*

### 1.2. [rpi_rootfs](https://github.com/kclyu/rpi_rootfs)
```
mkdir -p ~/Workspace
git clone https://github.com/kclyu/rpi_rootfs
```

### 1.3. 	Custom Compiled GCC for Raspberry PI

*If you are familiar with the process of cross compiling or already using your own cross compile method, ignore the rpi_rootfs and Custom Compiled GCC download. But, The procedure described below is based on rpi_rootfs, so please modify the necessary parts yourself.*
```
mkdir -p ~/Workspace
git clone https://github.com/kclyu/rpi_rootfs
cd rpi_rootfs
mkdir tools
cd tools
# (Download Custom Compiled GCC) # note1
xz -dc ~/Downloads/gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf.tar.xz  | tar xvf -
ln -sf gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf  arm-linux-gnueabihf
cd /opt
sudo ln -sf ~/Workspace/rpi_rootfs
export PATH=/opt/rpi_rootfs/tools/arm-linux-gnueabihf/bin:$PATH
```
*Note 1: Custom Compiled GCC : Please click  gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf.tar.xz link to download it. Because of the large file size, google drive link is available for download. You may get a warning message that "file size is too large to scan for viruses" and "You can not 'Preview'" during downloading from google drive.*


|URL|SHAsum|Remarks|
|----------------|---------------|------------|
|[gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf.tar.xz](https://drive.google.com/open?id=1s67nRSYZtLkIlRDz-BsDPkBXaTA94tsZ)|2b88b6c619e0b28f6493e1b7971c327574ffdb36|RASPBIAN STRETCH|


## 2. Download Source Repo
### 2.1. The directory name specified by source package
Currently, the directory name is fixed for each source package. If possible, use the directory name mentioned below, and if you want to change it, need to change the directory name specified in the RWS Makefile and script.


|Fixed directory name|source package/descriptions|
|----------------|-----------------|
|$(HOME)/Workspace/webrtc|WebRTC native-code package source|
|$(HOME)/Workspace/webrtc/src/arm_build|The WebRTC library and object created by WebRTC native-code package building|
|$(HOME)/Workspace/rpi-webrtc-streamer|Rpi-Webrtc-Streamer source|
|$(HOME)/Workspace/rpi_rootfs|Rootfs for Raspberry PI|


### 2.2. [WebRTC native-code package](https://webrtc.org/native-code/development/)

```
mkdir -p ~/Workspace/webrtc
cd ~/Workspace/webrtc
fetch --nohooks webrtc
gclient sync
git config branch.autosetupmerge always
git config branch.autosetuprebase always
cd src
git checkout master
```

### 2.3. [Rpi-WebRTC-Streamer](https://github.com/kclyu/rpi-webrtc-streamer)

```
cd ~/Workspace
git clone https://github.com/kclyu/rpi-webrtc-streamer
```

## 3. Installing Prerequisite s/w package

### 3.1. Installing Raspberry PI Prerequisite

```
ssh pi@your-raspberry-pi-ipaddress

sudo apt-get update && sudo apt-get upgrade
sudo apt-get install rsync
sudo apt-get install libasound2-dev libcairo2-dev  libffi-dev libglib2.0-dev  \
libgtk2.0-dev libpam0g-dev libpulse-dev  libudev-dev  libxtst-dev  \
ttf-dejavu-core libatk1.0-0 libc6 libasound2  libcairo2 libcap2 libcups2  \
libexpat1 libffi6 libfontconfig1 libfreetype6  libglib2.0-0 libgnome-keyring0  \
libgtk2.0-0 libpam0g libpango1.0-0  libpcre3 libpixman-1-0 libpng12-0 libstdc++6  \
libx11-6 libx11-xcb1 libxau6 libxcb1 libxcomposite1 libxcursor1 libxdamage1   \
libxdmcp6 libxext6 libxfixes3 libxi6 libxinerama1 libxrandr2 libxrender1  \
libxtst6 zlib1g gtk+-3.0 libavahi-client-dev
```

### 3.2. Installing prerequisite s/w package in Ubuntu Linux
```
cd ~/Workspace/webrtc/src/build
sudo ./install-build-deps.sh
```

## 4. Building WebRTC native-code library and RWS

### 4.1. Making Raspberry PI Root FS

```
cd ~/Workspace/rpi_rootfs
./rpi_rootfs.py pi@your-raspberry-pi-ipaddress ./
```
*When rpi_rootfs.py is executed, many messages are output to console during the sync list of files and library link fixing. Please ignore the messages.*

If you installed a new library or software on Raspberry PI, please execute it again to apply the changed part.


### 4.2. Building WebRTC native-code library
```
cd ~/Workspace/webrtc/src
mkdir arm_build
cp ~/Workspace/rpi-webrtc-stremer/misc/webrtc_arm_build_args.gn arm_build/args.gn #note 3
gn gen arm_build
ninja -C arm_build
```
*note 3 : webrtc_arm_build_args.gn is the args.gn config file for armv7 and above, and webrtc_armv6_build_args.gn  is for Raspberry Pi Zero W and armv6.*

### 4.3. Building Rpi-WebRTC-Streamer
```
cd ~/Workspace/rpi-webrtc-streamer/src
sh ../mk/config_libwebsockets.sh
make # note 4
```
*note 4 : If you get a compile error when you make RWS, please change the WebRTC native code package to the previous commit location using 'Moving to specific commit position of WebRTC native code package' below.*

### 4.4. Moving to specific commit position of WebRTC native code package
RWS has a commit log with the title 'Source update for WebRTC Cr-Commit Position'. This includes WebRTC's Cr-Commit-Position number and WebRTC commit position hash. (Recently modified)
If WebRTC commit hash is '4d22a6d8dbb1db1f459338c14f8c6d814c6c9678', WebRTC commit position can be changed by following command.

```
cd ~/Workspace/webrtc/src
gclient sync -n -D -r 4d22a6d8dbb1db1f459338c14f8c6d814c6c9678
gclient sync
```
If you have successfully changed the commit position, build the WebRTC native code package again as described below.

```
cd ~/Workspace/webrtc/src
gn clean arm_build
gn gen arm_build
ninja -C arm_build

# maing RWS again
cd ~/Workspace/rpi-webrtc-streamer/src
make clean
make
```

## 5. RWS setup and testing
If there were no compilation problems, the `webrtc-streamer` executable would have been created in ~ /Workspace/rpi-webrtc-streamer.
Copy the webrtc-streamer executable file and 'etc' and 'web-root' directory to raspberry pi.


### 5.1. Testing
Below is a description of how to test the new build webrtc-streamer binary on Raspberry Pi. This can be used to verify that the newly built binary is running normally, or to test the part that has been modified since the source was modified.

```
ssh pi@your-raspberry-pi-ipaddress mkdir -p /home/pi/Workspace/rws
scp -r etc tools log web-root webrtc-streamer pi@your-reaspberry-pi-ipaddress:/home/pi/Workspace/rws
ssh pi@your-raspberry-pi-ipaddress
cd ~/Workspace/rws
cp etc/template/*.conf etc
vi etc/webrtc_streamer.conf   # change "web_root=/opt/rws/web-root" to "web_root=/home/pi/Workspace/rws/web-root"
./webrtc-streamer --verbose
```

Open the following URL in the browser will display the native-peerconnection testing page.

```
http://your-raspberry-pi-ipaddress:8889/native-peerconnection/
```


### 5.2. Setup

Please refer to the [README_rws_setup document](../master/README_rws_setup.md).for rws execution and environment setting.



