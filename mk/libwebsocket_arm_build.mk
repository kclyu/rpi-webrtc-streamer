#
# libwebsocket compiler and linker definiton
#
#
LIBWEBSOCKETS_ROOT=$(HOME)/Workspace/rpi-webrtc-streamer/lib/libwebsockets

# CC FLAGS
LWS_C_FLAGS=-Wall -fvisibility=hidden -pthread
LWS_INCLUDES=-I$(LIBWEBSOCKETS_ROOT)/arm_build \
			 -I$(LIBWEBSOCKETS_ROOT)/arm_build/include \
			 -I$(LIBWEBSOCKETS_ROOT)

# LD FLAGS
LWS_LIBS=$(LIBWEBSOCKETS_ROOT)/arm_build/lib/libwebsockets.a
LWS_SYS_LIBS=-lz -lssl -lcrypto -lm
