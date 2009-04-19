# $Id$
#
# Configuration for Linux machines.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default compiler.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=

COMPILE_FLAGS+=-pthread
LINK_FLAGS+=-pthread
