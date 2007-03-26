# $Id$
#
# Configuration for FreeBSD.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default compiler.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=

# X11 location
#  Expect default of ports/packages.
#  Define X11BASE when different.
X11BASE?=/usr/X11R6

# ports/packages location
#  openMSX depends on various libraries.
#  Here we expect user installed libraries via ports/packages.
#  Expect default prefix of ports/packages.
#  Define PKGBASE when different.
PKGBASE?=/usr/local

COMPILE_FLAGS+=-D_REENTRANT -D_THREAD_SAFE \
	`if [ -d $(X11BASE)/include ]; then echo '-I$(X11BASE)/include'; fi` \
	`if [ -d $(PKGBASE)/include ]; then echo '-I$(PKGBASE)/include'; fi`

LINK_FLAGS+=-pthread \
	`if [ -d $(X11BASE)/lib ]; then echo '-L$(X11BASE)/lib'; fi` \
	`if [ -d $(PKGBASE)/lib ]; then echo '-L$(PKGBASE)/lib'; fi`


# Probe Overrides
# ===============

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>
