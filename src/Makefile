
# including make scripts supports RWS building
include ../mk/validate_variables.mk	 # validating required variable to build
include ../mk/cross_arm_build.mk
include ../mk/cross_mmal.mk
include ../mk/libwebsocket_arm_build.mk
include ../mk/libavahi_arm.mk

#
# including compat as source directory
#
VPATH = .:./compat

RWS_INSTALL_DIR=\"/opt/rws\"
RWS_VERSION=\"$(shell git describe --abbrev=16 --dirty --always --tags)\"
WEBRTC_GIT=$(shell (cd $(WEBRTC_ROOT)/src/; git describe --abbrev=16 --dirty --always --tags))
WEBRTC_CRPOSITION=$(shell (cd $(WEBRTC_ROOT)/src/;\
	git log -1 | gawk 'match($$0,/Cr-Commit-Position/){print $$0}' | sed 's/ //g'))

CCFLAGS += -DINSTALL_DIR=$(RWS_INSTALL_DIR) $(LWS_C_FLAGS) $(WEBRTC_CCFLAGS)
CFLAGS += -DINSTALL_DIR=$(RWS_INSTALL_DIR) $(WEBRTC_CFLAGS)

INCLUDES += $(LWS_INCLUDES) $(WEBRTC_DEFINES) $(WEBRTC_FLAGS_INCLUDES) \
			$(H264BITSTREAM_INCLUDES) $(MMAL_INCLUDES) $(AVAHI_INCLUDES) \
			-I$(SYSROOT)/usr/include/arm-linux-gnueabihf

# adding abseil-cpp include path in this makefile
INCLUDES += -I$(WEBRTC_ROOT)/src/third_party/abseil-cpp/

# for Raspbian Strech
BUILD_LIBS += $(WEBRTC_BUILD_LIBS) $(LWS_LIBS) $(AVAHI_LIBS)
SYSLIBS += $(LWS_SYS_LIBS) $(WEBRTC_SYSLIBS)
LDFLAGS += $(WEBRTC_LDFLAGS) $(MMAL_LDFLAGS)

# Adding Version Information
CCFLAGS += -D__RWS_VERSION__=$(RWS_VERSION) \
		   -D__WEBRTC_VERSION__="\"$(WEBRTC_GIT),$(WEBRTC_CRPOSITION)\""

#
# Debugging purpose only
#
ifdef VERBOSE
  $(info LWS_C_FLAGS is "$(LWS_C_FLAGS)")
  $(info WEBRTC_CCFLAGS is "$(WEBRTC_CCFLAGS)")
  $(info MMAL_CFLAGS is "$(MMAL_CFLAGS)")
  $(info RWS_INSTALL_DIR is "$(RWS_INSTALL_DIR)")
  $(info CFLAGS is "$(CFLAGS)")
  $(info CCFLAGS is  "$(CCFLAGS)")
  $(info INCLUDES is "$(INCLUDES)")
  $(info SYSLIBS is "$(SYSLIBS)")
  $(info LDFLAGS is "$(LDFLAGS)")
  $(info BUILD_LIBS is "$(BUILD_LIBS)")
endif

#
# Raspberry PI testing host
ifndef RPI_HOST_URL
RPI_HOST_URL=pi@10.0.0.11:~/Workspace/client
endif

# TARGET
#
TARGET = ../webrtc-streamer
TARGET_LWS_LIBS = $(LWS_LIBS)

#
# source & object list definition
#
SOURCES.CC = websocket_server.cc websocket_server_callback.cc app_clientinfo.cc \
	app_channel.cc  direct_socket.cc  \
	raspi_decoder_dummy.cc raspi_decoder.cc raspi_encoder.cc raspi_encoder_impl.cc \
	mmal_wrapper.cc utils.cc streamer_observer.cc streamer.cc main.cc \
	raspi_quality_config.cc file_logger.cc app_ws_client.cc \
	raspi_motion.cc raspi_motionvector.cc raspi_motionblob.cc raspi_motionfile.cc \
	config_media.cc  config_streamer.cc  config_motion.cc \
	utils_pc_config.cc utils_pc_strings.cc \
	optionsfile.cc directory_iterator.cc file.cc file_posix.cc filestream.cc platform_file.cc


SOURCES.C = websocket_server_util.c mmal_video.c mmal_video_reset.c mmal_util.c raspicli.c \
	raspicamcontrol.c raspipreview.c mdns_publish.c

OBJECTS.CC = $(SOURCES.CC:.cc=.o)
OBJECTS.CPP = $(SOURCES.CPP:.cpp=.o)
OBJECTS.C = $(SOURCES.C:.c=.o)
OBJECTS = $(OBJECTS.CC) $(OBJECTS.C) $(OBJECTS.CPP)

all: $(TARGET_LWS_LIBS) $(TARGET)

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
	rm -f *.o  *.o.d  *.dwo $(TARGET)

distclean: clean
	rm -fr ../lib/libwebsockets

$(TARGET_LWS_LIBS) :
	../mk/config_libwebsockets.sh $(RPI_ROOTFS)

#
#  helper rules
#
rcp: $(TARGET)
	rsync -v -u -r --stats $(TARGET) *.cc *.h *.dwo compat $(RPI_HOST_URL)

rcp_full: $(TARGET)
	rsync -v -u -r --stats $(TARGET) ../LICENSE ../tools ../etc ../etc/template ../web-root \
		../scripts $(RPI_HOST_URL)

web: $(TARGET)
	rsync -v -u -r --stats ../web-root  $(RPI_HOST_URL)

#
# You need to install clang-format-9 
format:
	find . -maxdepth 2 -iname \*.h -o -iname \*.cc -o -iname \*.c \
		| xargs clang-format-9 -style=file -i -fallback-style=none

#
# You need to configure ccls and bear(2.4.3 >= version) to generate compile_commands.json
ccls:
	make clean
	bear --use-cc arm-linux-gnueabihf-gcc --use-c++ arm-linux-gnueabihf-g++  make
	rm -f ../compile_commands.json
	mv compile_commands.json ../

