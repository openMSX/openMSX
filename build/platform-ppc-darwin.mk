# $Id$
#
# Configuration for Darwin on PPC machines.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default build flavour.
OPENMSX_FLAVOUR?=ppc

# Default compiler.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=

# Probe Overrides
# ===============

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>
