
# Building RPi-WebRTC-Streamer in Ubuntu

## Cross Compile Environment Setup
Cross compile is an essential task in order to compile WebRTC native-code package and RWS(Rpi WebRTC Streamer). In fact, we can not download the WebRTC native-code package with the Raspberry PI.

#### Cross-compiler installation
To make cross compile environment on Ubuntu Linux, Please refer to [Embedded Linux Wiki](http://elinux.org/Main_Page) ['Cross Compile from Linux'](http://elinux.org/Raspberry_Pi_Kernel_Compilation#2._Cross_compiling_from_Linux) section in 'Raspberry PI Kernel Compilation'. 
Or, Simply follow the short guide might be found in [here](http://stackoverflow.com/questions/19162072/installing-raspberry-pi-cross-compiler/19269715#19269715)

Before going to next step,  verify that you can access the following command.
 - arm-linux-gnueabihf-gcc, 
 - arm-linux-gnueabihf-g++
 - arm-linux-gnueabihf-ar
 - arm-linux-gnueabihf-ld
 - arm-linux-gnueabihf-as
 - arm-linux-gnueabihf-ranlib

#### Raspberry PI sysroot 
Once the cross-compiler installation is complete, you must create a sysroot for the cross compile environment.  please refer to this [rpi_rootfs](https://github.com/kclyu/rpi_rootfs.git) repo.


## Notes before download source code
First, it seems better to describe the location of the directory that developer use before you download the source code. There may be a location is fixed in current code and script itself, if possible, it is recommended to start at the same directory location during compilation. (After confirming that there is no fixed location issue, this section will be removed.)

|package|directory|
|----------------|-----------------|
|WebRTC native-code|$(HOME)/Workspace/webrtc|
|Rpi-Webrtc-Streamer|$(HOME)/Workspace/rpi-webrtc-streamer|
|Rpi sysroot|$(HOME)/Workspace/rpi_rootfs|


## Building WebRTC native-code package
#### Install Prerequisite Software tool
To build the WebRTC native-code package, [Prerequisite Software tool](https://webrtc.org/native-code/development/prerequisite-sw/)  must be installed at first.

#### WebRTC native code package branch notice 
In fact, the WebRTC native code package adds code commit almost every day. Because the RWS repo can not be modified daily according to the WebRTC native code package, which is updated daily, this repo is modified to match the specific WebRTC native code branch.

The WebRTC branch for RWS compilation should use the branch version in the misc/WEBRTC_BRANCH.conf file.

#### Download WebRTC native-code package 
To download webrtc source code please use the following command: 

```
mkdir -p ~/Workspace/webrtc
cd ~/Workspace/webrtc
fetch --nohooks webrtc
cd src
gclient sync
git checkout -b rel55 branch-heads/55
gclient sync
```
When the syncing is complete, in order to verify, re-enter the following command **gclient sync** . check the following message comes out. 

```
$ gclient sync
Syncing projects: 100% (2/2), done.                      

________ running '/usr/bin/python -c import os,sys;script = os.path.join("trunk","check_root_dir.py");_ = os.system("%s %s" % (sys.executable,script)) if os.path.exists(script) else 0' in '/home/kclyu/Workspace/webrtc'

________ running '/usr/bin/python -u src/sync_chromium.py --target-revision 316b880c55452eb694a27ba4d1aa9e74ec9ef342' in '/home/kclyu/Workspace/webrtc'
Chromium already up to date:  316b880c55452eb694a27ba4d1aa9e74ec9ef342

________ running '/usr/bin/python src/setup_links.py' in '/home/kclyu/Workspace/webrtc'

________ running '/usr/bin/python src/build/landmines.py --landmine-scripts src/webrtc/build/get_landmines.py --src-dir src' in '/home/kclyu/Workspace/webrtc'

________ running '/usr/bin/python src/third_party/instrumented_libraries/scripts/download_binaries.py' in '/home/kclyu/Workspace/webrtc'

________ running 'download_from_google_storage --directory --recursive --num_threads=10 --no_auth --quiet --bucket chromium-webrtc-resources src/resources' in '/home/kclyu/Workspace/webrtc'
```
## Building WebRTC with _GN build_
#### Building WebRTC native-code package

_WebRTC native-code package start to use only GN build, it does not use GYP build anymore thereafter branch/54. you have to use GN if you want to use the latest WebRTC native-code package._ 


1. generate ninja build 
  
```
cd ~/Workspace/webrtc/src
mkdir arm_build
cp ~/Workspace/rpi-webrtc-streamer/misc/webrtc_build_args.gn arm_build/args.gn
gn gen arm_build   
```
- note:  You need to replace the whole absolute path in args.gn before doing 'gn gen arm_build'

3. building WebRTC library

the below command will start to build.
```
ninja -C arm_build
```
After compilation is finished without an error, go to the next step to compile rws.

## Building RWS(rpi-webrtc-streamer)
  
*  Setup Makefile and cross_*.mk 
 
|Item|file|description|
|----------------|-----------------|-----|
|Builder Type|Makefile|cross_gn.mk for GN builder, cross_gyp.mk for GYP builder|
|SYSROOT|cross_gn.mk|sysroot for raspberry pi |
|WEBRTCROOT|cross_gn.mk|WebRTC root directory path|
|WEBRTCOUTPUT|cross_gn.mk|WebRTC build output path|


   
*  build rws
 ```
cd ~/Workspace/
git clone https://github.com/kclyu/rpi-webrtc-streamer.git
cd rpi-webrtc-streamer/src
sh ../../scripts/config_h264bitstream.sh 
cd ..
make
```

## Version History
 * 2017/01/10 v0.57 : 
     - adding initial android direct socket feature
     - fixing branch-heads/55
     - removing unused GYP building scripts and files
     - webrtc build directory changed from 'arm/out/Debug' to 'arm_build'
 * 2016/09/20 v0.56 : Initial Version


