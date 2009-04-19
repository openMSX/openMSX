# $Id$
#
# Configuration for OpenBSD.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default compiler.
# GCC 3.3 or later is needed to compile openMSX. If you have an old OpenBSD,
# you might have to specify a different compiler than the systemwide one.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=
