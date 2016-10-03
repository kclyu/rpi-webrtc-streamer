##
##

##
##
export SYSROOT=$HOME/Workspace/rpi_rootfs
export SYSROOTFLAG="--sysroot=$SYSROOT"

##
##
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
## PATH : /opt/depot_tools:/opt/rpi_tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin
export CC="arm-linux-gnueabihf-gcc -O4 $SYSROOTFLAG $BACKTRACE $ARMIFY"
export CXX="arm-linux-gnueabihf-g++ -O4 $SYSROOTFLAG $BACKGRACE $ARMIFY"
export AR=arm-linux-gnueabihf-ar
export LD=arm-linux-gnueabihf-ld
export AS=arm-linux-gnueabihf-as
export RANLIB=arm-linux-gnueabihf-ranlib
