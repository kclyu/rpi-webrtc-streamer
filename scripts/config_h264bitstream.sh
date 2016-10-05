#!/bin/bash
export CC=arm-linux-gnueabihf-gcc 
export CXX=arm-linux-gnueabihf-g++ 
export AR=arm-linux-gnueabihf-ar
export LD=arm-linux-gnueabihf-ld
export AS=arm-linux-gnueabihf-as
export RANLIB=arm-linux-gnueabihf-ranlib

if [ -e ../misc/h264bitstream-0.1.9.tar.gz ]
then
	tar xvzf ../misc/h264bitstream-0.1.9.tar.gz
	mv h264bitstream-0.1.9 h264bitstream
	cd h264bitstream
	./configure --host=x86_64-unknown-linux-gnu --build=arm-linux-gnueabi --target=arm-linux-gnueabi --with-sysroot=${HOME}/Workspace/rpi_rootfs
	make 
else
	echo "../misc/h264bitstream-0.1.9.tar.gz not found"
fi
	
