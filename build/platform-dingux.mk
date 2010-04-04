# $Id$
#
# Configuration for Dingux: Linux for Dingoo A320.

# Dingux is a Linux/uClibc system.
include build/platform-linux.mk

# Allow GMenu to identify our binary as an executable.
# Since the file system is FAT this cannot be done with a permission flag.
EXEEXT:=.dge

# Build a minimal set of components.
LINK_MODE:=3RD_STA_MIN

ifeq ($(OPENMSX_TARGET_CPU),mipsel)
# Automatically select the cross compiler from its default location.
ifeq ($(origin CXX),default)
CXX:=/opt/mipsel-linux-uclibc/usr/bin/mipsel-linux-uclibc-g++
endif
# Use MIPS32 instruction set.
COMPILE_FLAGS+=-march=mips32
endif
