#!/bin/bash
export CC=arm-linux-gnueabihf-gcc 
export CXX=arm-linux-gnueabihf-g++ 
export AR=arm-linux-gnueabihf-ar
export LD=arm-linux-gnueabihf-ld
export AS=arm-linux-gnueabihf-as
export RANLIB=arm-linux-gnueabihf-ranlib
./configure --host=x86_64-unknown-linux-gnu --build=arm-linux-gnueabi --target=arm-linux-gnueabi --with-sysroot=${HOME}/Workspace/rpi_rootfs
make 
