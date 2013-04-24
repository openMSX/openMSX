# Configuration for Chrome Native Client (NaCl).

# Pick the right compiler.
ifeq ($(OPENMSX_TARGET_CPU),x86)
NACL_CPU:=i686
else
NACL_CPU:=$(OPENMSX_TARGET_CPU)
endif
CXX:=$(NACL_CPU)-nacl-g++

# Does platform support symlinks?
USE_SYMLINK:=false

# File name extension of executables.
EXEEXT:=.nexe
LIBRARYEXT:=.so

# Build a minimal set of components.
LINK_MODE:=3RD_STA_MIN
