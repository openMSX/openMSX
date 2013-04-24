# Configuration for Android, for ARM.
# It *must* be called from Android SDL port build system. Toolchain params
# like CXX and CXXFLAGS have been properly set-up by the SDL port build system
# before invoking this make file
#
ifeq ($(origin ANDROID_CXXFLAGS),undefined)
$(error Android build can only be invoked from SDL Android port build system. See compile.html for more details)
endif

# Does platform require symlinks? (it is used to link the openMSX executable
# from a location inside the $PATH, which means it is not applicable for
# Android platform))
USE_SYMLINK:=false

# For Android, a shared library must eventually be build
EXEEXT:=
LIBRARYEXT:=.so

TARGET_FLAGS:=$(ANDROID_LDFLAGS)

# Build a maximum set of components.
# See configure.py for LINK_MODE definition and usage
LINK_MODE:=3RD_STA_GLES
