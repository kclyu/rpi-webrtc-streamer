#
#
#
include ../mk/cross_arm_build.mk
include ../mk/cross_mmal.mk
include ../mk/libwebsocket_arm_build.mk

#
# fakevideocapturer.cc has been added and we need to compile this file
#
VPATH = .:$(WEBRTC_ROOT)/src/media/base:./compat

RWS_INSTALL_DIR=\"/opt/rws\"
RWS_VERSION=\"$(shell git describe --abbrev=32 --dirty --always --tags)\"
WEBRTC_GIT=$(shell (cd $(WEBRTC_ROOT)/src/; git describe --abbrev=32 --dirty --always --tags))
WEBRTC_CRPOSITION=$(shell (cd $(WEBRTC_ROOT)/src/;\
	git log -1 | gawk 'match($$0,/Cr-Commit-Position/){print $$0}' | sed 's/ //g'))


CCFLAGS += -DINSTALL_DIR=$(RWS_INSTALL_DIR) $(LWS_C_FLAGS) $(WEBRTC_CCFLAGS)
CFLAGS += -DINSTALL_DIR=$(RWS_INSTALL_DIR) $(WEBRTC_CFLAGS)

INCLUDES += $(LWS_INCLUDES) $(WEBRTC_DEFINES) $(WEBRTC_FLAGS_INCLUDES) $(H264BITSTREAM_INCLUDES) \
			$(MMAL_INCLUDES)

# adding abseil-cpp include path in this makefile
INCLUDES += -I$(WEBRTC_ROOT)/src/third_party/abseil-cpp/

# for Raspbian Strech
#BUILD_LIBS += $(WEBRTC_BUILD_LIBS) $(LWS_LIBS)
# for Raspbian jessie
BUILD_LIBS += $(WEBRTC_BUILD_LIBS) $(LWS_LIBS) /opt/rpi_rootfs/tools/arm-linux-gnueabihf/arm-linux-gnueabihf/libc/lib/libstdc++.a

SYSLIBS += $(LWS_SYS_LIBS) $(WEBRTC_SYSLIBS)
LDFLAGS += $(WEBRTC_LDFLAGS) $(MMAL_LDFLAGS)

# Adding Version Information
CCFLAGS += -D__RWS_VERSION__=$(RWS_VERSION) \
		   -D__WEBRTC_VERSION__="\"$(WEBRTC_GIT),$(WEBRTC_CRPOSITION)\""

#
# Debugging purpose only
#
#$(info LWS_C_FLAGS is "$(LWS_C_FLAGS)")
#$(info WEBRTC_CCFLAGS is "$(WEBRTC_CCFLAGS)")
#$(info MMAL_CFLAGS is "$(MMAL_CFLAGS)")
#$(info RWS_INSTALL_DIR is "$(RWS_INSTALL_DIR)")
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
SOURCES.CC = websocket_server.cc websocket_server_callback.cc app_clientinfo.cc \
	app_channel.cc  direct_socket.cc  \
	raspi_decoder_dummy.cc raspi_decoder.cc raspi_encoder.cc raspi_encoder_impl.cc \
	mmal_wrapper.cc utils.cc streamer_observer.cc streamer.cc main.cc \
	raspi_quality_config.cc file_logger.cc app_ws_client.cc \
	raspi_motion.cc raspi_motionvector.cc raspi_motionblob.cc raspi_motionfile.cc \
	config_media.cc  config_streamer.cc  config_motion.cc fakevideocapturer_notaskqueue.cc \
	fakeframesource.cc optionsfile.cc

SOURCES.C = websocket_server_util.c mmal_video.c mmal_video_reset.c mmal_util.c raspicli.c \
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
	$(CC) -I. $(CFLAGS) $(INCLUDES) $(MMAL_CFLAGS) -D__MMAL_ENCODER_DEBUG__ -c $< -o $@

.cc.o : $(OBJECTS.CC)
	$(CXX)  -I. $(CFLAGS) $(CCFLAGS) $(MMAL_CFLAGS) $(INCLUDES)  -c $< -o $@


$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $(TARGET) -Wl,--start-group $(OBJECTS) $(BUILD_LIBS) -Wl,--end-group $(SYSLIBS)

clean:
	rm -f *.o  *.o.d $(TARGET)

rcp: $(TARGET)
	rsync -v -u -r --stats $(TARGET) *.cc *.h pi@10.0.0.11:~/Workspace/client

