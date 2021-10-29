# Configuration for OpenDingux on GCW Zero.

# Set CXX before including platform-linux.mk (see comments in platform-linux.mk)
ifeq ($(OPENMSX_TARGET_CPU),mipsel)
# Automatically select the cross compiler.
ifeq ($(origin CXX),default)
CXX:=mipsel-gcw0-linux-uclibc-g++
endif
endif

# OpenDingux is a Linux-based system.
include src_build/platform-linux.mk

# Build with OpenGL ES and Laserdisc support.
LINK_MODE:=3RD_STA_GLES
