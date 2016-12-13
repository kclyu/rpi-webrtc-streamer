
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

#### Download WebRTC native-code package 
To download webrtc source code please use the following command: 

```
mkdir -p ~/Workspace/webrtc
cd ~/Workspace/webrtc
fetch --nohooks webrtc
gclient sync
```
When the syncing is complete, in order to verify, re-enter the following command **gclient sync** . check the following message comes out. 

```
$ gclient sync
Syncing projects: 100% (2/2), done.                      

________ running '/usr/bin/python -c import os,sys;script = os.path.join("trunk","check_root_dir.py");_ = os.system("%s %s" % (sys.executable,script)) if os.path.exists(script) else 0' in '/home/kclyu/Workspace/webrtc'

________ running '/usr/bin/python -u src/sync_chromium.py --target-revision cede888c27275835e5aaadf5dac49379eb3ac106' in '/home/kclyu/Workspace/webrtc'
Chromium already up to date:  cede888c27275835e5aaadf5dac49379eb3ac106

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
gn gen arm/out/Debug 
```

2. change build setting 

the below command will open the vi editor.
```
gn args arm/out/Debug 
```
update the contents with the following.(You can find lastest args.gn file in misc/args.gn, 
        You need to replace '/home/kclyu/' with your account's home path.)
```
# Build arguments go here. Examples:
#   is_component_build = true
#   is_debug = false
# See "gn args <out_dir> --list" for available build arguments.

is_component_build=false
#is_official_build=true
#is_debug=false # Relese Version

host_toolchain="//build/toolchain/linux:arm"
target_os="linux"
target_cpu="arm"
arm_float_abi="hard"
arm_use_neon=true
arm_tune="cortex-a7"
is_clang=false
use_sysroot=true
target_sysroot="/home/kclyu/Workspace/rpi_rootfs"
system_libdir="/home/kclyu/Workspace/rpi_rootfs/usr/lib/arm-linux-gnueabihf"
gold_path = "/home/kclyu/tools/rpi_tools/arm-linux-gnueabihf/bin"

#
#
use_ozone=true
is_desktop_linux=false
rtc_desktop_capture_supported=false

#
#
rtc_build_with_neon=true
enable_nacl=false
enable_pdf=false
enable_webvr=false
rtc_use_h264=true
use_openh264=true
rtc_include_tests=false
rtc_desktop_capture_supported=false
rtc_include_pulse_audio=false
rtc_enable_protobuf=false
treat_warnings_as_errors=false

```
3. building WebRTC library

the below command will start to build.
```
ninja -C arm/out/Debug 
```
After compilation is finished without an error, go to the next step to compile rws.

## Building RWS(rpi-webrtc-streamer)
  
*  Setup Makefile and cross_*.mk 
 
|Item|file|description|
|----------------|-----------------|-----|
|Builder Type|Makefile|cross_gn.mk for GN builder, cross_gyp.mk for GYP builder|
|SYSROOT|cross_*.mk|sysroot for raspberry pi |
|WEBRTCROOT|cross_*.mk|WebRTC root directory path|
|WEBRTCOUTPUT|cross_*.mk|WebRTC build output path|


 
   
*  build rws
 ```
cd ~/Workspace/
git clone https://github.com/kclyu/rpi-webrtc-streamer.git
cd rpi-webrtc-streamer/src
sh ../../scripts/config_h264bitstream.sh 
cd ..
make
```

## Building WebRTC with _GYP build_
_GYP build is no longer used. Thiis GYP build should be used only if you need to use a branch/54 or previous version._

#### Building WebRTC native-code package
1. changing to last gyp version
  
```
cd ~/Workspace/webrtc/
git checkout -b rel54 branch-heads/54
gclient 
```
    
2. source environment variables 
```
##
export SYSROOT=$HOME/Workspace/
export SYSROOTFLAG="--sysroot=$SYSROOT"
	
##
export GYP_CROSSCOMPILE=1 
export GYP_DEFINES="OS=linux target_arch=arm clang=0 arm_version=7 arm_use_neon=1 disable_fatal_linker_warnings=1 target_sysroot=/home/kclyu/Workspace/rpi_rootfs"
export GYP_DEFINES="$GYP_DEFINES enable_tracing=1 disable_nacl=0 enable_svg=0 webrtc_h264=1 rtc_use_h264=1 use_v4l2_codec=1 rtc_initialize_ffmpeg=1 media_use_ffmpeg=1 disable_fatal_linker_warnings=1"
export GYP_GENERATOR_OUTPUT='arm'
	
##
##
export ARMIFY="-mfpu=neon-vfpv4 -mfloat-abi=hard -funsafe-math-optimizations"
export BACKTRACE="-funwind-tables -rdynamic"
	
##
## PATH : /opt/rpi_tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin
export CC="arm-linux-gnueabihf-gcc -O4 $SYSROOTFLAG $BACKTRACE $ARMIFY"
export CXX="arm-linux-gnueabihf-g++ -O4 $SYSROOTFLAG $BACKGRACE $ARMIFY"
export AR=arm-linux-gnueabihf-ar
export LD=arm-linux-gnueabihf-ld
export AS=arm-linux-gnueabihf-as
export RANLIB=arm-linux-gnueabihf-ranlib
```

3. source environment 
```
cd ~/Workspace/webrtc
gclient sync
ninja -C arm/out/Debug
```

## Version History
 * 2016/09/20 v0.56 : Initial Version


