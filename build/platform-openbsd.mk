# $Id$
#
# Configuration for OpenBSD.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default compiler.
# OpenBSD 3.6 RELEASE has GCC 2.95 as base, which is not capable of compiling
# openMSX. The egcc compiler is GCC 3.3.2.
OPENMSX_CXX?=eg++

# File name extension of executables.
EXEEXT:=

# Probe Overrides
# ===============

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>

TCL_CFLAGS=-I/usr/local/include/tcl8.4

