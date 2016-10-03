#!/bin/bash

if [ "$#" -ne 1 ]; then
	echo "usage ./cmd rpi_rootfs"
	exit -1
fi

ROOTFS=$1
TOOLCHAIN=arm-linux-gnueabihf

find ${ROOTFS}/usr/lib/arm-linux-gnueabihf/pkgconfig -printf "%f\n" | while read target 
do
	echo "../../lib/arm-linux-gnueabihf/pkgconfig/${target}" ${ROOTFS}/usr/share/pkgconfig/${target}
	ln -snfv "../../lib/arm-linux-gnueabihf/pkgconfig/${target}" ${ROOTFS}/usr/share/pkgconfig/${target}
done
