# Configuration for Android, for ARM.

# Does platform require symlinks? (it is used to link the openMSX executable
# from a location inside the $PATH, which means it is not applicable for
# Android platform))
USE_SYMLINK:=false

# For Android, a shared library must eventually be build
EXEEXT:=
LIBRARYEXT:=.so

TARGET_FLAGS:=-DANDROID -fPIC

LINK_FLAGS+=-llog
#LDFLAGS+=--no-undefined

# Build a maximum set of components.
# See configure.py for LINK_MODE definition and usage
LINK_MODE:=3RD_STA_GLES

# Automatically select the cross compiler.
ifeq ($(origin CXX),default)
ifeq ($(OPENMSX_TARGET_CPU),arm)
CXX:=armv7a-linux-androideabi18-clang++
else
CXX:=$(OPENMSX_TARGET_CPU)-linux-android-clang++
endif
endif
