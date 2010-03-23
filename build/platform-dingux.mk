# $Id$
#
# Configuration for Dingux: Linux for Dingoo A320.

# Dingux is a Linux/uClibc system.
include build/platform-linux.mk

# Automatically select the cross compiler from its default location.
ifeq ($(OPENMSX_TARGET_CPU),mipsel)
CXX?=/opt/mipsel-linux-uclibc/usr/bin/mipsel-linux-uclibc-g++
endif
