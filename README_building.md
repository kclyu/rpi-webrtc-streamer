# Building RPi-WebRTC-Streamer in Ubuntu

This Rpi-WebRTC-Streamer (RWS) builing document describes the procedure for cross compiling RWS in Ubuntu Linux.
For more information on tools or source repo, please refer to the linked URL and do not explain it separately.

## 1. Required Tools

### 1.1. [depot_tools](http://dev.chromium.org/developers/how-tos/depottools)

```
mkdir ~/tools
cd ~/tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:$HOME/tools/depot_tools
```

_You can use the directory named tools in the desired directory. Just add depot_tools to your path so that you can access it directly from the command line._

### 1.2. [rpi_rootfs](https://github.com/kclyu/rpi_rootfs)

```
mkdir -p ~/Workspace
git clone https://github.com/kclyu/rpi_rootfs
```

### 1.3. Custom Compiled GCC for Raspberry PI

_If you are familiar with the process of cross compiling or already using your own cross compile method, ignore the rpi_rootfs and Custom Compiled GCC download. But, The procedure described below is based on rpi_rootfs, so please modify the necessary parts yourself._

```
cd ~/Workspace/rpi_rootfs
mkdir tools
cd tools
../scripts/gdrive_download.sh 1q7Zk-7NhVROrBBWVgm56PbndZauSZL27 gcc-linaro-8.3.0-2019.03-x86_64_arm-linux-gnueabihf.tar.xz
#...
#Saving to: ‘gcc-linaro-8.3.0-2019.03-x86_64_arm-linux-gnueabihf.tar.xz’
#...
xz -dc gcc-linaro-8.3.0-2019.03-x86_64_arm-linux-gnueabihf.tar.xz  | tar xvf -
ln -sf gcc-linaro-8.3.0-2019.03-x86_64_arm-linux-gnueabihf  arm-linux-gnueabihf
cd /opt
sudo ln -sf ~/Workspace/rpi_rootfs
export PATH=/opt/rpi_rootfs/tools/arm-linux-gnueabihf/bin:$PATH
```


| URL                                                                                                                              | md5sum                           | Remarks         |
| -------------------------------------------------------------------------------------------------------------------------------- | -------------------------------- | --------------- |
| [gcc-linaro-8.3.0-2019.03-x86_64_arm-linux-gnueabihf.tar.xz](https://drive.google.com/open?id=1q7Zk-7NhVROrBBWVgm56PbndZauSZL27) | 633025d696d55ca0a3a099be8e34db23 | Raspbian Buster |

## 2. Download Source Repo

### 2.1. The directory name specified by source package

Currently, the directory name is fixed for each source package. If possible, use the directory name mentioned below, and if you want to change it, need to change the directory name specified in the RWS Makefile and script.

| Fixed directory name                    | source package/descriptions                                                  |
| --------------------------------------- | ---------------------------------------------------------------------------- |
| \$(HOME)/Workspace/webrtc               | WebRTC native-code package source                                            |
| \$(HOME)/Workspace/webrtc/src/out/arm_build | The WebRTC library and object created by WebRTC native-code package building for arm|
| \$(HOME)/Workspace/webrtc/src/out/armv6_build | The WebRTC library and object created by WebRTC native-code package building for armv6 |
| \$(HOME)/Workspace/rpi-webrtc-streamer  | Rpi-Webrtc-Streamer source                                                   |
| \$(HOME)/Workspace/rpi_rootfs/rootfs    | Rootfs for Raspberry PI                                                      |

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


## 3. Building WebRTC native-code library and RWS

### 3.1. Making Raspberry PI Root FS

```
cd ~/Workspace/rpi_rootfs
./build_rootfs.sh download   # note1
unzip yyyy-mm-dd-raspbian-buster.zip  # yyyy-mm-dd is date format of image released

./build_rootfs.sh create yyyy-mm-dd-raspbian-buster.img  # note 2
```
_Note 1: If you already have an OS image, you don't need to download it_

_Note 2: The create command will create raspberry pi rootfs in the rootfs directory using the downloaded img file. s/w packages are updated by 'apt update' and necessary s/w packages will be   installed also, so it takes a lot of time depending on network speed._

If you are already using Raspberry PI, there is a way to use rsync on your Raspberry PI rather than the above. See the rpi_rootfs repo.

### 3.2. Building WebRTC native-code library

```
sudo apt install ant autoconf bison cmake gawk intltool xutils-dev xsltproc pkg-config # note 1
cd ~/Workspace/webrtc/src
mkdir out/arm_build
cp ~/Workspace/rpi-webrtc-stremer/misc/webrtc_arm_build_args.gn out/arm_build/args.gn # note 2
gn gen out/arm_build
ninja -C out/arm_build

mkdir out/armv6_build # note 3
cp ~/Workspace/rpi-webrtc-stremer/misc/webrtc_armv6_build_args.gn out/armv6_build/args.gn # note 2
gn gen out/armv6_build
ninja -C out/armv6_build
```
_note 1 : S/W package required to compile WebRTC native code package in Ubuntu._

_note 2 : webrtc_arm_build_args.gn is the args.gn config file for armv7 and above, and webrtc_armv6_build_args.gn is for Raspberry Pi Zero W and armv6._

_note 3 : If you do not need rws building for armv6, the following is not necessary._

### 3.3. Building Rpi-WebRTC-Streamer

```
cd ~/Workspace/rpi-webrtc-streamer/src
make # note 1

make clean
make ARCH=armv6 # armv6 building
```

_note 1 : If you get a compile error when you make RWS, please change the WebRTC native code package to the previous commit location using 'Moving to specific commit position of WebRTC native code package' below._

### 3.4. Moving to specific commit position of WebRTC native code package

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
gn clean out/arm_build
gn gen out/arm_build
ninja -C out/arm_build

# maing RWS again
cd ~/Workspace/rpi-webrtc-streamer/src
make clean
make
```

## 4. RWS setup and testing

If there were no compilation problems, the `webrtc-streamer` executable would have been created in ~ /Workspace/rpi-webrtc-streamer.
Copy the webrtc-streamer executable file and 'etc' and 'web-root' directory to raspberry pi.

### 4.1. Testing

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
http://your-raspberry-pi-ipaddress:8889/np2/
```

### 4.2. Setup

Please refer to the [README_rws_setup document](../master/README_rws_setup.md).for rws execution and environment setting.
