# $Id$
#
# Configuration for Linux on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default build flavour.
OPENMSX_FLAVOUR?=i686

# Default compiler.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=

# TODO: Temp to force g++ 3.4-pre to pick up the right libs.
ifeq ($(OPENMSX_FLAVOUR),gcc34)
LINK_FLAGS:=-L/opt/gcc-cvs/lib $(LINK_FLAGS)
endif
