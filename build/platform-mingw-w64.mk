# Configuration for MinGW-w64.
# Don't be fooled by the name: it can compile to both 32-bit and 64-bit Windows.
#  http://mingw-w64.sourceforge.net/

include build/platform-mingw32.mk

# File name extension of executables.... AGAIN.
# Workaround because the makevar parsing doesn't do includes
EXEEXT:=.exe

ifeq ($(OPENMSX_TARGET_CPU),x86)
MINGW_CPU:=i686
else
MINGW_CPU:=$(OPENMSX_TARGET_CPU)
endif

CXX:=$(MINGW_CPU)-w64-mingw32-g++

# Native windres is not prefixed; just use default from main.mk there.
ifeq ($(filter MINGW%,$(shell uname -s)),)
WINDRES?=$(MINGW_CPU)-w64-mingw32-windres
endif

# Statically link the third-party bundle when that build mode is selected.
# System MinGW packages provide import libraries (for example Tcl), which
# cannot be used with a blanket -static option.
ifeq ($(LINK_MODE),3RD_STA)
LINK_FLAGS:= -static $(LINK_FLAGS)
endif
