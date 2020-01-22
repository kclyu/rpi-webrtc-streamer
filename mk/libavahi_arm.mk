#
# Avahi library compiler and linker flags definiton
#
AVAHI_INCLUDES=-I$(SYSROOT)/usr/include

# LD FLAGS
AVAHI_LIBS=-L$(SYSROOT)/usr/lib -lavahi-client -lavahi-common
