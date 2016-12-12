#
# 
#
include ../misc/cross_gn.mk

#
# h264bitstream & openssl library
#
H264BITSTREAM_LIB = ../lib/h264bitstream/.libs/libh264bitstream.a
OPENSSL_LIB = -lssl -lcrypto
CCFLAGS +=  -I../lib

#
# Variables for MMAL(raspivid) compilation
#
PI_INCLUDE=$(SYSROOT)/opt/vc/include
MMAL_CFLAGS+=-Wno-multichar -Wno-unused-but-set-variable -fPIC -O3 -DDEBUG -DEGL_SERVER_DISPMANX -DHAVE_CMAKE_CONFIG -DHAVE_VMCS_CONFIG -DOMX_SKIP64BIT -DTV_SUPPORTED_MODE_NO_DEPRECATED -DUSE_VCHIQ_ARM -DVCHI_BULK_ALIGN=1 -DVCHI_BULK_GRANULARITY=1 -D_FILE_OFFSET_BITS=64 -D_HAVE_SBRK -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -D_REENTRANT -D__VIDEOCORE4__ -D__WEBRTC_DEFAULT__

MMAL_INCLUDES+=-I$(PI_INCLUDE)/interface/vcos/pthreads -I$(PI_INCLUDE)/interface/vmcs_host/linux -I$(PI_INCLUDE)/interface/vmcs_host -I$(PI_INCLUDE)/interface/vmcs_host/khronos -I$(PI_INCLUDE)/interface/khronos/include -I$(PI_INCLUDE)/interface/vchiq_arm -I$(PI_INCLUDE) -I$(PI_INCLUDE)/interface/mmal 

MMAL_LDFLAGS+=-L$(SYSROOT)/opt/vc/lib/ -lmmal_core -lmmal -lmmal_util -lvcos -lcontainers -lbcm_host -lmmal_vc_client -lmmal_components -lvchiq_arm -lvcsm -lvcfiled_check -lpthread -ldl -lrt -lm 

#
# websocketpp Compiler Definition
#
#WSPP_INCLUDE=-I../lib/websocketpp -I$(SYSROOT)/usr/local/include -D_WEBSOCKETPP_CPP11_STL_
#WSPP_CCFLAGS= -fno-threadsafe-statics -fvisibility-inlines-hidden -std=gnu++11 -Wno-narrowing -fno-rtti -Wnon-virtual-dtor -Woverloaded-virtual
#WSPP_LDFLAGS= $(SYSROOT)/usr/local/boost/lib/libboost_system.a $(SYSROOT)/usr/local/boost/lib/libboost_thread.a $(SYSROOT)/usr/local/boost/lib/libboost_random.a -Wl,-rpath,$(SYSROOT)/usr/local/boost/lib
#UWEBSOCKET=$(HOME)/Workspace/uWebSockets
#WSPP_INCLUDE=-I$(UWEBSOCKET)/src -I$(SYSROOT)/usr/local/include 
#WSPP_CCFLAGS= -fno-threadsafe-statics -fvisibility-inlines-hidden -std=gnu++11 -Wno-narrowing -fno-rtti -Wnon-virtual-dtor -Woverloaded-virtual
#WSPP_LDFLAGS= $(UWEBSOCKET)/build/libuWS.a -Wl,-Bstatic -luv -Wl,-Bdynamic $(OPENSSL_LIB) -lz

#
# TARGET
#
TARGET = ../webrtc-streamer

#
# source & object list definition
#
SOURCES.CC = stream_data_sockets.cc stream_sockets.cc direct_socket.cc streamer.cc streamer_defaults.cc main.cc raspi_encoder.cc mmal_wrapper.cc h264bitstream_parser.cc utils.cc
#SOURCES.CPP = wspp_client.cpp
SOURCES.C = mmal_encoder.c mmal_encoder_reset.c mmal_util.c raspicli.c raspicamcontrol.c raspipreview.c 


OBJECTS.CC = $(SOURCES.CC:.cc=.o) 
OBJECTS.CPP = $(SOURCES.CPP:.cpp=.o) 
OBJECTS.C = $(SOURCES.C:.c=.o)
OBJECTS = $(OBJECTS.CC) $(OBJECTS.C) $(OBJECTS.CPP)


all: $(TARGET)

#
# Makefile rules...
#
.c.o : $(OBJECTS.C) 
	$(CC) -I. $(CFLAGS) $(MMAL_CFLAGS) $(MMAL_INCLUDES) -c $< -o $@

.cc.o : $(OBJECTS.CC) 
	$(CXX) -I. $(CFLAGS) $(CCFLAGS) $(CCFLAGS_DEFS) $(INCLUDE_WEBRTC_LIB) $(MMAL_CFLAGS) $(MMAL_INCLUDES)  -c $< -o $@

.cpp.o : $(OBJECTS.CCP) 
	$(CXX) -I. $(WSPP_CCFLAGS) $(WSPP_INCLUDE) -c $< -o $@

$(TARGET): $(OBJECTS) 
	# TODO will core dump when ssl library linked with webrtc-streamer
	#$(CXX) $(LDFLAGS) -o $(TARGET) $(CFLAGS) $(CCFLAGS) -Wl,--start-group $(OBJECTS) $(INCLUDE_WEBRTC_LIB_LIST) $(MMAL_LDFLAGS) $(H264BITSTREAM_LIB) -Wl,--end-group $(OPENSSL_LIB) $(SYS_LIBS) 
	$(CXX) $(LDFLAGS) -o $(TARGET) $(CFLAGS) $(CCFLAGS) -Wl,--start-group $(OBJECTS) $(INCLUDE_WEBRTC_LIB_LIST) $(MMAL_LDFLAGS) $(H264BITSTREAM_LIB) -Wl,--end-group $(OPENSSL_LIB) $(SYS_LIBS) 

clean:
	rm -f *.o $(TARGET)

rcp: $(TARGET)
	rsync -v -u -r --stats ../webrtc-streamer  pi@10.0.0.11:~/Workspace/client

