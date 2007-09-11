# $Id$
#
# Configuration for NetBSD.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default compiler.
OPENMSX_CXX?=$(CXX)

# File name extension of executables.
EXEEXT:=

COMPILE_FLAGS+=-D_REENTRANT -D_THREAD_SAFE
LINK_FLAGS+=-pthread

# Probe Overrides
# ===============

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>

# Apparently libpng-config is not available.
PNG_CFLAGS_SYS_DYN:=
PNG_LDFLAGS_SYS_DYN:=-lpng
PNG_RESULT_SYS_DYN:=yes
