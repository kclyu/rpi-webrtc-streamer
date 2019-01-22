#!/bin/bash
export CC=arm-linux-gnueabihf-gcc
export CXX=arm-linux-gnueabihf-g++
export AR=arm-linux-gnueabihf-ar
export LD=arm-linux-gnueabihf-ld
export AS=arm-linux-gnueabihf-as
export RANLIB=arm-linux-gnueabihf-ranlib


RWS_LIBRARY_DIR=../lib
RPI_ROOTFS_CMAKE=${HOME}/Workspace/rpi_rootfs/PI.cmake

LIBWEBSOCKETS_BASENAME=libwebsockets-3.1.0
LIBWEBSOCKET_DIR=${RWS_LIBRARY_DIR}/libwebsockets
LIBWEBSOCKET_BUILD_DIR=${RWS_LIBRARY_DIR}/libwebsockets/arm_build
LIBWEBSOCKETS_LIBRARY=${LIBWEBSOCKET_BUILD_DIR}/lib/libwebsockets.a

##
## Check whether rpi_rootfs repo exist
if [ ! -e ${RPI_ROOTFS_CMAKE} ]
then
	echo "rpi_rootfs does not exists"
	echo "You need to configure rpi_rootfs repo to build"
	echo "Please check https://github.com/kclyu/rpi_rootfs"
    exit -1
fi

if [ -e ${RWS_LIBRARY_DIR}/${LIBWEBSOCKETS_BASENAME}.zip ]
then
    # checking libwebsockets library build directory exist
    if [ ! -d ${LIBWEBSOCKET_DIR} ]
    then
	    echo "extracting libwebsocket library in lib"
        cd ${RWS_LIBRARY_DIR} && unzip ${LIBWEBSOCKETS_BASENAME}.zip && mv ${LIBWEBSOCKETS_BASENAME} ${LIBWEBSOCKET_DIR}
    fi

    # checking libwebsockets library archive file
    if [ ! -f ${LIBWEBSOCKETS_LIBRARY} ]
    then
	    echo "start building libwebsockets library"
        mkdir -p ${LIBWEBSOCKET_BUILD_DIR}
        cd ${LIBWEBSOCKET_BUILD_DIR} && cmake .. -DCMAKE_TOOLCHAIN_FILE=${RPI_ROOTFS_CMAKE} -DCMAKE_BUILD_TYPE=Debug -DLWS_WITH_SSL=OFF -DLWS_WITH_SHARED=OFF -DLWS_WITH_LIBEV=0 && make
    else
	    echo "libwebsockets.a already exist"
    fi
else
	echo "Zipfile ${RWS_LIBRARY_DIR}/${LIBWEBSOCKETS_BASENAME}.zip not found"
fi
exit 0

