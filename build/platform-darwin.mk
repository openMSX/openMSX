# $Id$
#
# Configuration for Darwin.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default compiler.
OPENMSX_CXX?=g++

# Compile for the selected CPU.
ifeq ($(OPENMSX_TARGET_CPU),x86)
TARGET_FLAGS+=-arch i386
else
TARGET_FLAGS+=-arch $(OPENMSX_TARGET_CPU)
endif

# File name extension of executables.
EXEEXT:=

# Bind when executable is loaded, rather then when symbols are accessed.
# I don't know why, but the linker suggests this.
LINK_FLAGS+=-bind_at_load

# Select the SDK for the OS X version we want to be compatible with.
ifeq ($(OPENMSX_TARGET_CPU),ppc)
SDK_PATH:=$(firstword $(sort $(wildcard /Developer/SDKs/MacOSX10.3.?.sdk)))
OPENMSX_CXX:=g++-3.3
else
SDK_PATH:=/Developer/SDKs/MacOSX10.4u.sdk
endif
export NEXT_ROOT=$(SDK_PATH)

# When NEXT_ROOT is defined, /usr/lib will not be scanned for libraries by
# default, but users might have installed some dependencies there.
LINK_FLAGS+=-L/usr/lib


# Probe Overrides
# ===============

DIR_IF_EXISTS=$(shell test -d $(1) && echo $(1))

# DarwinPorts library and header paths.
DARWINPORTS_CFLAGS:=$(addprefix -I,$(call DIR_IF_EXISTS,/opt/local/include))
DARWINPORTS_LDFLAGS:=$(addprefix -L,$(call DIR_IF_EXISTS,/opt/local/lib))

# Fink library and header paths.
FINK_CFLAGS:=$(addprefix -I,$(call DIR_IF_EXISTS,/sw/include))
FINK_LDFLAGS:=$(addprefix -L,$(call DIR_IF_EXISTS,/sw/lib))

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>
# TODO:
# GL_HEADER:=<OpenGL/gl.h> iso GL_CFLAGS is cleaner,
# but we have to modify the build before we can use it.
GL_CFLAGS:=-I/System/Library/Frameworks/OpenGL.framework/Headers
GL_LDFLAGS:=-framework OpenGL -lGL \
	-L/System/Library/Frameworks/OpenGL.framework/Libraries

GLEW_CFLAGS+=$(DARWINPORTS_CFLAGS) $(FINK_CFLAGS)
GL_GLEW_CFLAGS+=$(DARWINPORTS_CFLAGS) $(FINK_CFLAGS)
GLEW_LDFLAGS+=$(DARWINPORTS_LDFLAGS) $(FINK_LDFLAGS)
