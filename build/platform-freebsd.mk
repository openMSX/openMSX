# $Id$
#
# Configuration for FreeBSD.

# Does platform support symlinks?
USE_SYMLINK:=true

# File name extension of executables.
EXEEXT:=

# ports/packages location
#  openMSX depends on various libraries.
#  Here we expect user installed libraries via ports/packages.
#  Expect default prefix of ports/packages.
#  Define PKGBASE when different.
PKGBASE?=/usr/local

COMPILE_FLAGS+=-D_REENTRANT -D_THREAD_SAFE -I$(PKGBASE)/include

LINK_FLAGS+=-pthread -L$(PKGBASE)/lib
