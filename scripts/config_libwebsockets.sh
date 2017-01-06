#!/bin/bash
export CC=arm-linux-gnueabihf-gcc 
export CXX=arm-linux-gnueabihf-g++ 
export AR=arm-linux-gnueabihf-ar
export LD=arm-linux-gnueabihf-ld
export AS=arm-linux-gnueabihf-as
export RANLIB=arm-linux-gnueabihf-ranlib

if [ -e ../misc/libwebsockets-master.zip ]
then
    # checking libwebsockets library directory  
    if [ ! -d ../lib/libwebsockets ]
    then
	    echo "extracting libwebsocket library in lib"
        cd ../lib && unzip  ../misc/libwebsockets-master.zip && mv libwebsockets-master libwebsockets
        mkdir -p libwebsockets/rpi_build
        # 
    fi

    pwd
    # checking libwebsockets library archive file
    if [ ! -f ../lib/libwebsockets/rpi_build/lib/libwebsockets.a ]
    then
	    echo "start building libwebsockets library"
        cd ../lib/libwebsockets/rpi_build && cmake -DCMAKE_TOOLCHAIN_FILE=~/Workspace/rpi_rootfs/PI.cmake  -DCMAKE_BUILD_TYPE=Debug .. -DLWS_WITH_SHARED=OFF && make
    else
	    echo "libwebsockets.a already exist"
    fi
else
	echo "../misc/libwebsocket-master.zip not found"
fi
	
