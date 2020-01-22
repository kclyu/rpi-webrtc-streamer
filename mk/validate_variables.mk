#
# ARCH can only be used in either arm or armv6.
# 	arm : above armv6
# 	armv6 : armv6 (raspberry pi zero)
ARCH=arm
ifeq ($(ARCH),arm)
  TARGET_ARCH_BUILD=arm_build
else ifeq ($(ARCH),armv6)
  TARGET_ARCH_BUILD=armv6_build
else
  $(error Unknown ARCH $(ARCH) type argument)
endif

#
# Checking RPI_ROOTFS variable
#
ifndef RPI_ROOTFS
  # default RPI_ROOTFS variable
  RPI_ROOTFS=/opt/rpi_rootfs
endif
ifeq ("$(wildcard $(RPI_ROOTFS))","")
define RPI_ROOTFS_ERROR_MESG
Rpi_RootFS is not configured, please configure rpi_rootfs
You need to configure rpi_rootfs repo to build,Please check https://github.com/kclyu/rpi_rootfs
endef
  $(error $(RPI_ROOTFS_ERROR_MESG))
endif

#
# Checking WEBRTC_ROOT variable
#
ifndef WEBRTC_ROOT
  # default WEBRTC_ROOT variable
  WEBRTC_ROOT=$(HOME)/Workspace/webrtc
endif
ifeq ("$(wildcard $(WEBRTC_ROOT)/src)","")
define WEBRTC_ROOT_ERROR_MESG
WebRTC native source code package is not configured, please configure it before try RWS building
endef
  $(error $(WEBRTC_ROOT_ERROR_MESG))
endif

