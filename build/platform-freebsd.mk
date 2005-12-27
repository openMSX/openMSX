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

CXXFLAGS+=-D_REENTRANT -D_THREAD_SAFE \
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

# sdl-config script
#  openMSX depends on SDL.
#  Here we expect user installed SDL via ports/packages.
#  Meta-port "sdl_ldbad" makes symlink to sdl11-config as sdl-config.
#  But for users who use packages only, here use sdl11-config from ports/packages "sdl12".
#  Why "11" even "sdl12" ? We don't know. Ask to maintainer of "sdl12" ports/packages...
SDLCONFIGSCRIPT?=$(PKGBASE)/bin/sdl11-config

SDL_CFLAGS:=`$(SDLCONFIGSCRIPT) --cflags 2>> $(LOG)`

SDL_LDFLAGS:=`$(SDLCONFIGSCRIPT) --libs 2>> $(LOG)`
SDL_RESULT:=`$(SDLCONFIGSCRIPT) --version`

# libpng related.
#  openMSX depends on libpng.
#  Here we expect user installed libpng via ports/packages.
#  Fortunately, ports/packages make symlinks
#  $(PKGBASE)/include/libpng/*.h to $(PKGBASE)/include/*.h .
#  So, just use CXXFLAGS and LINK_FLAGS set in above.
#  Should use "$(PKGBASE)/libdata/pkgconfig/libpng12.pc" ?
PNG_CFLAGS:=
PNG_LDFLAGS:=-lpng
PNG_RESULT:=yes
