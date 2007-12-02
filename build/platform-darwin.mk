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

# These libraries are part of the base system, therefore we do not need to
# link them statically for building a redistributable binary.
SYSTEM_LIBS:=ZLIB TCL XML

# Select the SDK for the OS X version we want to be compatible with.
# TODO: When compiling an executable for local use, we could pick the OS X
#       version we are running on instead of the oldest version we support.
#       But at the moment I don't want to make this more complex than it
#       already is.
ifeq ($(OPENMSX_TARGET_CPU),ppc)
SDK_PATH:=$(firstword $(sort $(wildcard /Developer/SDKs/MacOSX10.3.?.sdk)))
OSX_VER:=10.3
OSX_MIN_REQ:=1030
else
SDK_PATH:=/Developer/SDKs/MacOSX10.4u.sdk
OSX_VER:=10.4
OSX_MIN_REQ:=1040
endif
COMPILE_ENV+=NEXT_ROOT=$(SDK_PATH) MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)
LINK_ENV+=NEXT_ROOT=$(SDK_PATH) MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)
TARGET_FLAGS+=-D__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__=$(OSX_MIN_REQ)
TARGET_FLAGS+=-isysroot $(SDK_PATH)
LINK_FLAGS+=-Wl,-syslibroot,$(SDK_PATH)

ifeq ($(filter 3RD_%,$(LINK_MODE)),)
# Compile against local libs. We assume the binary is intended to be run on
# this Mac only.
# Note that even though we compile for local use, we still have to compile
# against the SDK since OS X 10.3 will have link problems otherwise (the
# QuickTime framework in particular is notorious for this).

# When NEXT_ROOT is defined, /usr/lib will not be scanned for libraries by
# default, but users might have installed some dependencies there.
LINK_FLAGS+=-L/usr/lib
else
# Compile against 3rdparty libs. We assume the binary is intended to be run
# on different Mac OS X versions, so we compile against the SDKs for the
# system-wide libraries.

# Nothing to define.
endif


# Probe Overrides
# ===============

DIR_IF_EXISTS=$(shell test -d $(1) && echo $(1))

# MacPorts library and header paths.
MACPORTS_CFLAGS:=$(addprefix -I,$(call DIR_IF_EXISTS,/opt/local/include))
MACPORTS_LDFLAGS:=$(addprefix -L,$(call DIR_IF_EXISTS,/opt/local/lib))

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

GLEW_CFLAGS_SYS_DYN+=$(MACPORTS_CFLAGS) $(FINK_CFLAGS)
GLEW_LDFLAGS_SYS_DYN+=$(MACPORTS_LDFLAGS) $(FINK_LDFLAGS)

SDL_LDFLAGS_3RD_STA:=`$(3RDPARTY_INSTALL_DIR)/bin/sdl-config --static-libs 2>> $(LOG)`

ifneq ($(filter 3RD_%,$(LINK_MODE)),)
# Use tclConfig.sh from /usr: ideally we would use tclConfig.sh from the SDK,
# but the SDK doesn't contain that file. The -isysroot compiler argument makes
# sure the headers are taken from the SDK though.
TCL_CFLAGS_SYS_DYN:=`build/tcl-search.sh --cflags 2>> $(LOG)`
TCL_LDFLAGS_SYS_DYN:=`build/tcl-search.sh --ldflags 2>> $(LOG)`

# Use xml2-config from /usr: ideally we would use xml2-config from the SDK,
# but the SDK doesn't contain that file. The -isysroot compiler argument makes
# sure the headers are taken from the SDK though.
XML_CFLAGS_SYS_DYN:=`/usr/bin/xml2-config --cflags 2>> $(LOG)`
XML_LDFLAGS_SYS_DYN:=`/usr/bin/xml2-config --libs 2>> $(LOG)`
XML_RESULT_SYS_DYN:=`/usr/bin/xml2-config --version`
endif
