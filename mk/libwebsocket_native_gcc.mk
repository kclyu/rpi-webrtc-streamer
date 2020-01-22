#
# libwebsocket compiler and linker definiton
#
#
LIBWEBSOCKETS_ROOT=$(HOME)/Workspace/rpi-webrtc-streamer/lib/libwebsockets


# CC FLAGS
#LWS_C_FLAGS=-Wall -Werror -fvisibility=hidden -pthread
LWS_C_FLAGS=-Wall -fvisibility=hidden -pthread
LWS_INCLUDES=-I$(LIBWEBSOCKETS_ROOT)/linux_build

# LD FLAGS
LWS_LIBS=$(LIBWEBSOCKETS_ROOT)/linux_build/lib/libwebsockets.a
LWS_SYS_LIBS=-lz -lssl -lcrypto -lm
