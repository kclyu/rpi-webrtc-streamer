
# Building RPi-WebRTC-Streamer in Ubuntu

## Cross Compile Environment Setup
Cross compile is an essential task in order to compile WebRTC native-code package and RWS(Rpi WebRTC Streamer). In fact, we can not download the WebRTC native-code package with the Raspberry PI.

### Making Raspberry PI sysroot image
RWS will be compiled using the Raspbery PI sysroot created through rpi_rootfs. For additional procedures, please refer to the [Raspberry PI rootfs Repo](https://github.com/kclyu/rpi_rootfs).

### Cross-compiler installation


 [rpi_rootfs repo](https://github.com/kclyu/rpi_rootfs.git) have cross compiler for  raspberry pi, After cloning rpi_rootfs in the appropriate directory, please link to /opt/rpi_rootfs and add /opt/rpi_rootfs/tools/arm-linux-gnueabihf/bin in your PATH environment variable to use cross compiler.


```
cd ~/Workspace
git clone https://github.com/kclyu/rpi_rootfs.git
cd rpi_rootfs
tar tvzf tools_gcc_4.9.4.tar.gz  # note1
cd /opt
sudo ln -sf ~/Workspace/rpi_rootfs
```
-- Note1  downloading tools_gcc_4.9.4.tar.gz 

**Git LFS currently has a monthly download limit. If you are having trouble downloading, please go to google drive file link below. ( You may get a warning message that "file size is too large to scan for viruses" and "You can not 'Preview'".**

|URL|SHA256sum|
|----------------|---------------|
|[tools_gcc-4.9.4.tar.gz](https://drive.google.com/open?id=0B4FN-EnejHTaLWVILVFkVTZteWM)|99e0aa822ff8bcdd3bbfe978466f2bed281001553f3b9c295eba2d6ed636d6c2|





```
$ /opt/rpi_rootfs/tools/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc -v
Using built-in specs.
COLLECT_GCC=/opt/rpi_rootfs/tools/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
COLLECT_LTO_WRAPPER=/opt/rpi_rootfs/tools/arm-linux-gnueabihf/libexec/gcc/arm-linux-gnueabihf/4.9.4/lto-wrapper
Target: arm-linux-gnueabihf
Configured with: /home/kclyu/Workspace/cross/.build/src/gcc-linaro-4.9-2015.06/configure --build=x86_64-build_pc-linux-gnu --host=x86_64-build_pc-linux-gnu --target=arm-linux-gnueabihf --prefix=/opt/rpi_rootfs/tools/arm-linux-gnueabihf --with-sysroot=/opt/rpi_rootfs/tools/arm-linux-gnueabihf/arm-linux-gnueabihf/sysroot --enable-languages=c,c++ --with-arch=armv6 --with-tune=arm1176jz-s --with-fpu=vfp --with-float=hard --with-pkgversion='crosstool-NG crosstool-ng-1.22.0-248-gdf5a341 - Linaro GCC 2015.06' --with-bugurl=https://bugs.launchpad.net/gcc-linaro --disable-__cxa_atexit --disable-libmudflap --enable-libgomp --disable-libssp --enable-libquadmath --enable-libquadmath-support --disable-libsanitizer --with-gmp=/home/kclyu/Workspace/cross/.build/arm-linux-gnueabihf/buildtools --with-mpfr=/home/kclyu/Workspace/cross/.build/arm-linux-gnueabihf/buildtools --with-mpc=/home/kclyu/Workspace/cross/.build/arm-linux-gnueabihf/buildtools --with-isl=/home/kclyu/Workspace/cross/.build/arm-linux-gnueabihf/buildtools --with-cloog=/home/kclyu/Workspace/cross/.build/arm-linux-gnueabihf/buildtools --enable-lto --with-host-libstdcxx='-static-libgcc -Wl,-Bstatic,-lstdc++,-Bdynamic -lm' --enable-threads=posix --disable-libstdcxx-pch --enable-linker-build-id --with-linker-hash-style=gnu --enable-plugin --enable-gold --disable-multilib --with-local-prefix=/opt/rpi_rootfs/tools/arm-linux-gnueabihf/arm-linux-gnueabihf/sysroot --enable-long-long --with-arch=armv6 --with-float=hard --with-fpu=vfp --enable-multiarch
Thread model: posix
gcc version 4.9.4 20150629 (prerelease) (crosstool-NG crosstool-ng-1.22.0-248-gdf5a341 - Linaro GCC 2015.06) 

```
Before going to next step, verify that you can access the following command.

- arm-linux-gnueabihf-gcc,
- arm-linux-gnueabihf-g++
- arm-linux-gnueabihf-ar
- arm-linux-gnueabihf-ld
- arm-linux-gnueabihf-as
- arm-linux-gnueabihf-ranlib


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

#### Notice about WebRTC native code package branch 
In fact, the WebRTC native code package is committed almost daily. So, It is virtually impossible to keep the rws code up to date in WebRTC native code that is modified daily. This repo will be used to update the code on a branch-by-branch basis.

If you are using a master clone or any other branch of rws, be sure to check the WebRTC branch currently used by rws and use the corresponding WebRTC branch.
Check the misc/WEBRTC_BRANCH.conf for the WebRTC branch used in rws and use the corresponding WebRTC branch.


#### Download specific branch of WebRTC native-code package
To download WebRTC native source package, please use the following command: 

```
mkdir -p ~/Workspace/webrtc
cd ~/Workspace/webrtc
fetch --nohooks webrtc
cd src
gclient sync
```
**note:** the above command example use 'master' branch of WebRTC native code package. 

When the syncing is completed, in order to verify, re-enter the following command **gclient sync** . check the following message comes out. 

```
$ gclient sync

________ running '/usr/bin/python src/cleanup_links.py' in '/home/kclyu/Workspace/webrtc'
Syncing projects: 100% (42/42), done.                                             

...
...
...

________ running 'download_from_google_storage --directory --recursive --num_threads=10 --no_auth --quiet --bucket chromium-webrtc-resources src/resources' in '/home/kclyu/Workspace/webrtc'
Hook 'download_from_google_storage --directory --recursive --num_threads=10 --no_auth --quiet --bucket chromium-webrtc-resources src/resources' took 33.07 secs

```

## Building WebRTC with _GN build_
#### Building WebRTC native-code package

_WebRTC native-code package start to use only GN build, it does not use GYP build anymore thereafter branch/54. you have to use GN if you want to use the latest WebRTC native-code package._ 


1. generate ninja build 
  
```
cd ~/Workspace/
git clone https://github.com/kclyu/rpi-webrtc-streamer.git
cd ~/Workspace/webrtc/src
mkdir arm_build
cp ~/Workspace/rpi-webrtc-streamer/misc/webrtc_arm_build_args.gn arm_build/args.gn
gn gen arm_build   
```

2. building WebRTC library

the below command will start to build.
```
ninja -C arm_build
```
After compilation is finished without an error, go to the next step to compile rws.

## Building RWS(rpi-webrtc-streamer)
  
   
1. build RWS

RWS uses third party libraries. Before running make to build RWS, you must first create the libraries using config_h264bitstream.sh and config_libwebsockets.sh. For a real example, see the command example below.
 ```
cd ~/Workspace/rpi-webrtc-streamer/src
sh ../mk/config_h264bitstream.sh
sh ../mk/config_libwebsockets.sh 
make
```

 - Setup Makefile and cross_arm_gn.mk 
 
If you use the working directory mentioned above (i.e. ~/Workspace/webrtc and ~/Workspace/rpi_rootfs, ~/Workspace/rpi-webrtc-streamer), you don't need to modify the following variables. If you use a different directory path, you need to modify the following variables. 

 
|Item|file|description|
|----------------|-----------------|-----|
|SYSROOT|cross_arm_gn.mk|sysroot for raspberry pi |
|WEBRTCROOT|cross_arm_gn.mk|WebRTC root directory path|
|WEBRTCOUTPUT|cross_arm_gn.mk|WebRTC build output path|


## RWS setup
If there were no compilation problems, the webrtc-streamer executable would have been created in ~ /Workspace/rpi-webrtc-streamer.
Copy the webrtc-streamer executable file and 'etc' and 'web-root' directory to raspberry pi. Please refer to the [README_rws_setup document](../master/README_rws_setup.md).for rws execution and environment setting.