
#
# Variables for MMAL(raspivid) compilation
#
PI_INCLUDE=$(SYSROOT)/opt/vc/include
MMAL_CFLAGS+=-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX \
			-D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE \
			-D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 \
			-DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX \
			-DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM

MMAL_INCLUDES+=-I$(PI_INCLUDE)/interface/vcos/pthreads \
			-I$(PI_INCLUDE)/interface/vmcs_host/linux \
			-I$(PI_INCLUDE)/interface/vmcs_host \
			-I$(PI_INCLUDE)/interface/vchiq_arm \
			-I$(PI_INCLUDE) -I$(PI_INCLUDE)/interface/mmal

MMAL_LDFLAGS+=-Wl,--no-as-needed -L$(SYSROOT)/opt/vc/lib/ -lmmal_core -lmmal \
			  -lmmal_util -lvcos -lbcm_host -lmmal_vc_client -lmmal_components \
			  -lvchiq_arm

