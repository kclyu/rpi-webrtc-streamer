#
# h264bitstream compiler and linker definiton 
# 
#
H264BITSTREAM_ROOT=$(HOME)/Workspace/rpi-webrtc-streamer/lib/h264bitstream


# CC FLAGS
H264BITSTREAM_INCLUDES=-I$(H264BITSTREAM_ROOT)/..

# LD FLAGS
H264_BITSTREAM_LIBS=$(H264BITSTREAM_ROOT)/.libs/libh264bitstream.a

