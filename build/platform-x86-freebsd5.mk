# $Id$
#
# Configuration for FreeBSD 5.x on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default build flavour.
OPENMSX_FLAVOUR?=i686

# Default compiler.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=

LINK_FLAGS+=-L/usr/X11R6/lib


# Probe Overrides
# ===============

SDL_CFLAGS:=`sdl11-config --cflags 2>> $(LOG)`

SDL_LDFLAGS:=`sdl11-config --libs 2>> $(LOG)`
SDL_RESULT:=`sdl11-config --version`

PNG_CFLAGS:=

PNG_LDFLAGS:=-lpng
PNG_RESULT:=yes
