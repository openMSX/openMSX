# $Id$
#
# Configuration for creating a Darwin app folder.
# In practice, this is used for Mac OS X; I'm not sure all of it applies to
# other Darwin-based systems.

# Does platform support symlinks?
USE_SYMLINK:=true

# The app folder will set a hi-res icon, so the openMSX process should not
# replace this with its own low-res icon.
SET_WINDOW_ICON:=false

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
SDK_PATH:=/Developer/SDKs/MacOSX10.4u.sdk
OSX_VER:=10.4
OSX_MIN_REQ:=1040
COMPILE_ENV+=NEXT_ROOT=$(SDK_PATH) MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)
LINK_ENV+=NEXT_ROOT=$(SDK_PATH) MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)
TARGET_FLAGS+=-D__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__=$(OSX_MIN_REQ)
TARGET_FLAGS+=-isysroot $(SDK_PATH)
LINK_FLAGS+=-Wl,-syslibroot,$(SDK_PATH)
# The OS X 10.4 SDK does not have the GCC 4.2 headers.
CXX:=g++-4.0

ifeq ($(filter 3RD_%,$(LINK_MODE)),)
# Compile against local libs. We assume the binary is intended to be run on
# this Mac only.
# Note that even though we compile for local use, we still have to compile
# against the SDK since OS X 10.3 will have link problems otherwise (the
# QuickTime framework in particular is notorious for this).

# TODO: Verify whether this is how to do it.
COMPILE_FLAGS+=-I/usr/local/include

# When NEXT_ROOT is defined, /usr/lib will not be scanned for libraries by
# default, but users might have installed some dependencies there.
LINK_FLAGS+=-L/usr/lib
else
# Compile against 3rdparty libs. We assume the binary is intended to be run
# on different Mac OS X versions, so we compile against the SDKs for the
# system-wide libraries.

# Nothing to define.
endif
