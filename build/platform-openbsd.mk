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

# Probe Overrides
# ===============

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>

# TODO: tcl-search.sh should provide a proper value for CFLAGS, so do we really
#       need this?
TCL_CFLAGS_SYS_DYN:=-I/usr/local/include/tcl8.4
