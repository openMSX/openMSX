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

# Libraries that do not have a lib-config script.
LIBS_PLAIN:=SDL_image png GL z
# Libraries that have a lib-config script.
LIBS_CONFIG:=xml2 sdl11

LINK_FLAGS+=-L/usr/X11R6/lib
