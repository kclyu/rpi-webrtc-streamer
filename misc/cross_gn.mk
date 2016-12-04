SYSROOT=/home/kclyu/Workspace/rpi_rootfs
SYSROOTFLAG=--sysroot=$(SYSROOT)
ARMIFY=-mfpu=neon-vfpv4 -mfloat-abi=hard -funsafe-math-optimizations
BACKTRACE=-funwind-tables -rdynamic


CC=arm-linux-gnueabihf-gcc $(SYSROOTFLAG) $(BACKTRACE) $(ARMIFY)
CXX=arm-linux-gnueabihf-g++ -fPIC $(SYSROOTFLAG) $(BACKGRACE) $(ARMIFY)
AR=arm-linux-gnueabihf-ar
LD=arm-linux-gnueabihf-ld
AS=arm-linux-gnueabihf-as
RANLIB=arm-linux-gnueabihf-ranlib

#CC=arm-rpi-linux-gnueabihf-gcc $(SYSROOTFLAG) $(BACKTRACE) $(ARMIFY)
#CXX=arm-rpi-linux-gnueabihf-g++ -fPIC $(SYSROOTFLAG) $(BACKGRACE) $(ARMIFY)
#AR=arm-rpi-linux-gnueabihf-ar
#LD=arm-rpi-linux-gnueabihf-ld
#AS=arm-rpi-linux-gnueabihf-as
#RANLIB=arm-rpi-linux-gnueabihf-ranlib

#
# WebRTC source tree and object directory 
# 
WEBRTCROOT=$(HOME)/Workspace/webrtc
WEBRTCOUTPUT=arm/out/Debug
WEBRTCLIBPATH=$(WEBRTCROOT)/src/$(WEBRTCOUTPUT)

## WebRTC library build script
# GN Build
WEBRTC_LIBRARY_BUILD=../scripts/webrtc_library_gn.py $(WEBRTCROOT) $(WEBRTCOUTPUT)

CFLAGS = -g -W -pthread -fPIC 
#CFLAGS = $(shell $(WEBRTC_LIBRARY_BUILD) cflags)

CCFLAGS_DEFS =-DDEBUG -Igen $(shell $(WEBRTC_LIBRARY_BUILD) defines)

CCFLAGS = $(shell $(WEBRTC_LIBRARY_BUILD) ccflags)

CCFLAGS += -Wl,--export-dynamic -pthread -fno-strict-aliasing  -Wl,-z,noexecstack
CCFLAGS += -I$(WEBRTCROOT)/src -I$(SYSROOT)/usr/lib/arm-linux-gnueabihf/glib-2.0/include -I$(WEBRTCROOT)/src/chromium/src/third_party/jsoncpp/source/include
#CCFLAGS += $(shell $(WEBRTC_LIBRARY_BUILD) includes)

# puluse_audio need X11 library
#SYS_LIBS += -lX11 -lXcomposite -lXext -lXrender -lpcre -lrt -lm -ldl -lexpat
SYS_LIBS += -lpcre -lrt -lm -ldl -lexpat
LDFLAGS += -Wl,-z,now -Wl,-z,relro -Wl,-z,defs -pthread -Wl,-z,noexecstack -fPIC -L$(SYSROOT)/usr/lib -L$(SYSROOT)/lib/ -L$(SYSROOT)/usr/lib/arm-linux-gnueabihf/ $(SYSROOTFLAG) -L$(SYSROOT)/lib/arm-linux-gnueabihf -Wl,-rpath-link=$(SYSROOT)/lib/arm-linux-gnueabihf -L$(SYSROOT)/usr/lib/arm-linux-gnueabihf -Wl,-rpath-link=$(SYSROOT)/usr/lib/arm-linux-gnueabihf 

INCLUDE_WEBRTC_LIB_LIST=$(shell $(WEBRTC_LIBRARY_BUILD) libs)

