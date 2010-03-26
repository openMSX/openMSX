# $Id$
#
# Configuration for Dingux: Linux for Dingoo A320.

# Dingux is a Linux/uClibc system.
include build/platform-linux.mk

# Build a minimal set of components.
LINK_MODE:=3RD_STA_MIN

# Automatically select the cross compiler from its default location.
ifeq ($(OPENMSX_TARGET_CPU),mipsel)
ifeq ($(origin CXX),default)
CXX:=/opt/mipsel-linux-uclibc/usr/bin/mipsel-linux-uclibc-g++
endif
endif
