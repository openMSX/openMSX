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

# Select the OS X version we want to be compatible with.
# TODO: When compiling an executable for local use, we could pick the OS X
#       version we are running on instead of the oldest version we support.
#       But at the moment I don't want to make this more complex than it
#       already is.
ifeq ($(OPENMSX_TARGET_CPU),x86_64)
OSX_MIN_VER:=10.6
else
OSX_MIN_VER:=10.4
endif

# Select the SDK to use. This can be higher than the OS X minimum version.
ifeq ($(OPENMSX_TARGET_CPU),x86_64)
SDK_PATH:=/Developer/SDKs/MacOSX10.6.sdk
else
# Note: Xcode 4.2 does not include PPC support in all of its dylibs, so when
#       building a PPC/universal binary we must use the SDK from Xcode 3.
#       If you have Xcode 3 installed in its default location (/Developer),
#       please remove the "Xcode3" part of the SDK_PATH definition below.
SDK_PATH:=/Developer/Xcode3/SDKs/MacOSX10.6.sdk
endif

# Compile against the SDK for the selected minimum OS X version.
COMPILE_ENV+=NEXT_ROOT=$(SDK_PATH) MACOSX_DEPLOYMENT_TARGET=$(OSX_MIN_VER)
LINK_ENV+=NEXT_ROOT=$(SDK_PATH) MACOSX_DEPLOYMENT_TARGET=$(OSX_MIN_VER)
TARGET_FLAGS+=-mmacosx-version-min=$(OSX_MIN_VER)
TARGET_FLAGS+=-isysroot $(SDK_PATH)
LINK_FLAGS+=-Wl,-syslibroot,$(SDK_PATH)

# Select an appropriate GCC version.
ifeq ($(OPENMSX_TARGET_CPU),x86_64)
CXX:=g++-4.2
else
# GCC from Xcode 4.2 fails to link PPC binaries, so use GCC from Xcode 3.
# GCC 4.2 on PPC and x86 uses a ridiculous amount of memory (about 7 GB)
# and will therefore never finish in a reasonable amount of time if the build
# machine has 4 GB of memory or less. GCC 4.0 does not have this problem.
CXX:=$(SDK_PATH)/../../usr/bin/g++-4.0
endif

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

# Link against CoreMIDI.
LINK_FLAGS+=-framework CoreMIDI
