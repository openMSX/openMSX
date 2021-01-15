# Configuration for creating a Darwin app folder.
# In practice, this is used for Mac OS X; I'm not sure all of it applies to
# other Darwin-based systems.

# Does platform support symlinks?
USE_SYMLINK:=true

# The app folder will set a hi-res icon, so the openMSX process should not
# replace this with its own low-res icon.
SET_WINDOW_ICON:=false

ifeq ($(OPENMSX_TARGET_CPU),aarch64)
TARGET_FLAGS+=-arch amd64
else
TARGET_FLAGS+=-arch $(OPENMSX_TARGET_CPU)
endif

# File name extension of executables.
EXEEXT:=
LIBRARYEXT:=.so

# Select the OS X version we want to be compatible with.
# In theory it is possible to compile against an OS X version number lower
# than the SDK version number, but in practice this doesn't seem to work
# since libraries such as libxml2 can change soname between OS X versions.
# Clang as shipped with Xcode requires OS X 10.7 or higher for compiling with
# libc++, when compiling Clang and libc++ from source 10.6 works as well.
OSX_VER:=10.13
TARGET_FLAGS+=-mmacosx-version-min=$(OSX_VER)

# Select Clang as the compiler and libc++ as the standard library.
CXX:=clang++
TARGET_FLAGS+=-stdlib=libc++

# Enable automatic reference counting in Objective-C
ifneq ($(3RDPARTY_FLAG),true)
TARGET_FLAGS+=-fobjc-arc
endif

# Link against system frameworks.
LINK_FLAGS+= \
	-framework CoreFoundation -framework CoreServices \
	-framework ApplicationServices -framework CoreMIDI \
	-framework Foundation
