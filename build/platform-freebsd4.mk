# $Id$
#
# Configuration for FreeBSD 4.x.

# Default compiler.
#  Default C++ compiler of FreeBSD-4 is g++ 2.9x.
#  openMSX requires g++3 or higher.
#  Here we expect the user (builder) has installed gcc33 via ports/packages.
OPENMSX_CXX?=g++33

include build/platform-freebsd.mk

# sdl-config script
#  openMSX depends on SDL.
#  Here we expect user installed SDL via ports/packages.
#  Meta-port "sdl_ldbad" makes symlink to sdl11-config as sdl-config.
#  But for users who use packages only, here use sdl11-config from ports/packages "sdl12".
#  Why "11" even "sdl12" ? We don't know. Ask to maintainer of "sdl12" ports/packages...
SDLCONFIGSCRIPT?=$(PKGBASE)/bin/sdl11-config
SDL_CFLAGS_SYS_DYN:=`$(SDLCONFIGSCRIPT) --cflags 2>> $(LOG)`
SDL_LDFLAGS_SYS_DYN:=`$(SDLCONFIGSCRIPT) --libs 2>> $(LOG)`
SDL_RESULT_SYS_DYN:=`$(SDLCONFIGSCRIPT) --version`

# libpng related.
#  openMSX depends on libpng.
#  Here we expect user installed libpng via ports/packages.
#  Fortunately, ports/packages make symlinks
#  $(PKGBASE)/include/libpng/*.h to $(PKGBASE)/include/*.h .
#  So, just use CXXFLAGS and LINK_FLAGS set in above.
#  Should use "$(PKGBASE)/libdata/pkgconfig/libpng12.pc" ?
PNG_CFLAGS_SYS_DYN:=
PNG_LDFLAGS_SYS_DYN:=-lpng
PNG_RESULT_SYS_DYN:=yes
