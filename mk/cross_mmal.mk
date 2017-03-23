
#
# Variables for MMAL(raspivid) compilation
#
PI_INCLUDE=$(SYSROOT)/opt/vc/include
MMAL_CFLAGS+=-Wno-multichar -Wno-unused-but-set-variable -fPIC -O3 -DDEBUG -DEGL_SERVER_DISPMANX -DHAVE_CMAKE_CONFIG -DHAVE_VMCS_CONFIG -DOMX_SKIP64BIT -DTV_SUPPORTED_MODE_NO_DEPRECATED -DUSE_VCHIQ_ARM -DVCHI_BULK_ALIGN=1 -DVCHI_BULK_GRANULARITY=1 -D_FILE_OFFSET_BITS=64 -D_HAVE_SBRK -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -D_REENTRANT -D__VIDEOCORE4__ -D__WEBRTC_DEFAULT__

MMAL_INCLUDES+=-I$(PI_INCLUDE)/interface/vcos/pthreads -I$(PI_INCLUDE)/interface/vmcs_host/linux -I$(PI_INCLUDE)/interface/vmcs_host -I$(PI_INCLUDE)/interface/vmcs_host/khronos -I$(PI_INCLUDE)/interface/khronos/include -I$(PI_INCLUDE)/interface/vchiq_arm -I$(PI_INCLUDE) -I$(PI_INCLUDE)/interface/mmal 

MMAL_LDFLAGS+=-L$(SYSROOT)/opt/vc/lib/ -lmmal_core -lmmal -lmmal_util -lvcos -lcontainers -lbcm_host -lmmal_vc_client -lmmal_components -lvchiq_arm -lvcsm -lvcfiled_check 

