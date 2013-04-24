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
LIBRARYEXT:=.so

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
TARGET_FLAGS+=-mmacosx-version-min=$(OSX_MIN_VER)

# Select the SDK to use. This can be higher than the OS X minimum version.
# The SDK path for Xcode from the Mac App Store:
SDK_PATH:=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk/
ifneq ($(shell [ -d $(SDK_PATH) ] && echo exists)$(filter $(OPENMSX_TARGET_CPU),ppc),exists)
# The SDK path for the older stand-alone Xcode:
SDK_PATH:=/Developer/SDKs/MacOSX10.6.sdk
endif
ifneq ($(shell [ -d $(SDK_PATH) ] && echo exists),exists)
$(error No Mac OS X SDK found)
endif
TARGET_FLAGS+=-isysroot $(SDK_PATH)

ifeq ($(OPENMSX_TARGET_CPU),ppc)
# Select an appropriate GCC version. Only PPC doesn't use clang yet.
CXX:=$(SDK_PATH)/../../usr/bin/g++-4.2
else
# Select clang as the compiler.
CXX:=clang++
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
