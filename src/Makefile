#
# 
#
include ../mk/cross_arm_build.mk
include ../mk/cross_mmal.mk
include ../mk/libwebsocket_arm_build.mk
include ../mk/h264bitstream_arm_build.mk

RWS_INSTALL_DIR=\"/opt/rws\"
RWS_VERSION=\"$(shell git describe --abbrev=8 --dirty --always --tags)\"
WEBRTC_GIT=$(shell (cd $(WEBRTC_ROOT)/src/; git describe --abbrev=8 --dirty --always --tags))
WEBRTC_NATIVE_PACKAGE=$(shell cat ../misc/WEBRTC_BRANCH.conf)


#
#
#
CCFLAGS += -DINSTALL_DIR=$(RWS_INSTALL_DIR) $(LWS_C_FLAGS) $(WEBRTC_CCFLAGS) $(MMAL_CFLAGS)
CFLAGS += -DINSTALL_DIR=$(RWS_INSTALL_DIR) $(WEBRTC_CFLAGS) $(MMAL_CFLAGS)
INCLUDES += $(LWS_INCLUDES) $(WEBRTC_DEFINES) $(WEBRTC_FLAGS_INCLUDES) $(H264BITSTREAM_INCLUDES) \
			$(MMAL_INCLUDES)
BUILD_LIBS += $(WEBRTC_BUILD_LIBS) $(LWS_LIBS) $(H264_BITSTREAM_LIBS)
SYSLIBS += $(LWS_SYS_LIBS) $(WEBRTC_SYSLIBS)
LDFLAGS += $(WEBRTC_LDFLAGS) $(MMAL_LDFLAGS) 

# Adding Version Inforation
CCFLAGS += -D__RWS_VERSION__=$(RWS_VERSION) \
		   -D__WEBRTC_VERSION__="\"$(WEBRTC_GIT),$(WEBRTC_NATIVE_PACKAGE)\""

#$(info CFLAGS is "$(CFLAGS)")
#$(info CCFLAGS is  "$(CCFLAGS)")
#$(info INCLUDES is "$(INCLUDES)")
#$(info SYSLIBS is "$(SYSLIBS)")
#$(info LDFLAGS is "$(LDFLAGS)")
#$(info BUILD_LIBS is "$(BUILD_LIBS)")


# TARGET
#
TARGET = ../webrtc-streamer

#
# source & object list definition
#
SOURCES.CC = websocket_server.cc websocket_server_callback.cc app_channel.cc \
	app_client.cc direct_socket.cc streamer_observer.cc streamer.cc \
	streamer_config.cc main.cc raspi_encoder.cc mmal_wrapper.cc \
	h264bitstream_parser.cc utils.cc raspi_quality_config.cc \
	media_config.cc 
SOURCES.C = websocket_server_util.c mmal_encoder.c mmal_encoder_reset.c mmal_util.c raspicli.c \
	raspicamcontrol.c raspipreview.c 


OBJECTS.CC = $(SOURCES.CC:.cc=.o) 
OBJECTS.CPP = $(SOURCES.CPP:.cpp=.o) 
OBJECTS.C = $(SOURCES.C:.c=.o)
OBJECTS = $(OBJECTS.CC) $(OBJECTS.C) $(OBJECTS.CPP)

all: $(TARGET)

#
# Makefile rules...
#
.c.o : $(OBJECTS.C) 
	$(CC) -I. $(CFLAGS) $(INCLUDES) -D__MMAL_ENCODER_DEBUG__ -c $< -o $@

.cc.o : $(OBJECTS.CC) 
	$(CXX)  -I. $(CFLAGS) $(CCFLAGS) $(INCLUDES)  -c $< -o $@


$(TARGET): $(OBJECTS) 
	$(CXX) $(LDFLAGS) -o $(TARGET) -Wl,--start-group $(OBJECTS) $(BUILD_LIBS) -Wl,--end-group $(SYSLIBS) 

clean:
	rm -f *.o  *.o.d $(TARGET)

rcp: $(TARGET)
	rsync -v -u -r --stats $(TARGET) *.cc *.h pi@10.0.0.11:~/Workspace/client

