# $Id$
#
# Configuration for NetBSD.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default compiler.
OPENMSX_CXX?=$(CXX)

# File name extension of executables.
EXEEXT:=

CXXFLAGS+=-D_REENTRANT -D_THREAD_SAFE
LINK_FLAGS+=-pthread

# Probe Overrides
# ===============

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>

PNG_CFLAGS:=

PNG_LDFLAGS:=-lpng
PNG_RESULT:=yes
