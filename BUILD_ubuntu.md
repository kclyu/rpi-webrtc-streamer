
# Building RPi-WebRTC-Streamer in Ubuntu

## Branch Notice
Notice: The RWS master repo is compiled based on the master branch of the WebRTC native code package. However, since WebRTC native code is being modified almost every day, sometimes you may have problems compiling RWS because it may already have many fixes added to the WebRTC master you have downloaded.

## Cross Compile Environment Setup
Cross compile is an essential task in order to compile WebRTC native-code package and RWS(Rpi WebRTC Streamer). In fact, we can not download the WebRTC native-code package with the Raspberry PI.

#### Cross-compiler installation
 [rpi_rootfs repo](https://github.com/kclyu/rpi_rootfs.git) have cross compiler for  raspberry pi, You don't need install cross compiler. After cloning rpi_rootfs in the appropriate directory, please link to /opt/rpi_rootfs and add /opt/rpi_rootfs/tools/arm-linux-gnueabihf/bin in your PATH environment variable to use cross compiler.

```
cd ~/Workspace
git clone https://github.com/kclyu/rpi_rootfs.git
cd /opt
sudo ln -sf ~/Workspace/rpi_rootfs
```

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


#### Raspberry PI sysroot 
Once the cross-compiler installation is complete, you must create a sysroot for cross compile environment.  please refer to this [rpi_rootfs](https://github.com/kclyu/rpi_rootfs.git) repo.


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
Check the misc / WEBRTC_BRANCH.conf for the WebRTC branch used in rws and use the corresponding WebRTC branch.


#### Download specific branch of WebRTC native-code package
To download webrtc source code please use the following command: 

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
|SYSROOT|cross_gn.mk|sysroot for raspberry pi |
|WEBRTCROOT|cross_gn.mk|WebRTC root directory path|
|WEBRTCOUTPUT|cross_gn.mk|WebRTC build output path|


   
*  build rws
 ```
cd ~/Workspace/
git clone https://github.com/kclyu/rpi-webrtc-streamer.git
cd rpi-webrtc-streamer/src
sh ../../scripts/config_h264bitstream.sh
make
```

## Version History
 * 2017/01/10 v0.57 : 
     - adding initial android direct socket feature
     - fixing branch-heads/55
     - removing unused GYP building scripts and files
     - webrtc build directory changed from 'arm/out/Debug' to 'arm_build'
 * 2016/09/20 v0.56 : Initial Version


