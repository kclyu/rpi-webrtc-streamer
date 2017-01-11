#!/bin/bash
export CC=arm-linux-gnueabihf-gcc 
export CXX=arm-linux-gnueabihf-g++ 
export AR=arm-linux-gnueabihf-ar
export LD=arm-linux-gnueabihf-ld
export AS=arm-linux-gnueabihf-as
export RANLIB=arm-linux-gnueabihf-ranlib

##
## Check whether rpi_rootfs repo exist
if [ -e ${HOME}/Workspace/rpi_rootfs/PI.cmake ]
then
	echo "rpi_rootfs does not exists"
	echo "You need to configure rpi_rootfs repo to build"
	echo "Please check https://github.com/kclyu/rpi_rootfs"
    exit -1
fi
if [ -e ../misc/h264bitstream-0.1.9.tar.gz ]
then
    # checking h264bitstream library directory  
    if [ ! -d ../lib/h264bitstream ]
    then
	    echo "extracting h264bitstream library in lib"
        cd ../lib && tar xvzf ../misc/h264bitstream-0.1.9.tar.gz && mv h264bitstream-0.1.9 h264bitstream
    fi

    # checking h264bitstream library archive file
    if [ ! -f ../lib/h264bitstream/.libs/libh264bitstream.a ]
    then
	    echo "start building h264bitstream library"
        cd ../lib/h264bitstream && ./configure --host=x86_64-unknown-linux-gnu --build=arm-linux-gnueabi --target=arm-linux-gnueabi --with-sysroot=${HOME}/Workspace/rpi_rootfs && make 
    else
	    echo "h264bitstream.a already exist"
    fi
else
	echo "../misc/h264bitstream-0.1.9.tar.gz not found"
fi
	
