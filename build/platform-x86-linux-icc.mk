# $Id$
#
# Configuration for Linux on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default build flavour.
OPENMSX_FLAVOUR?=i686

# Default compiler.
OPENMSX_CXX?=icc

# File name extension of executables.
EXEEXT:=

# Libraries that do not have a lib-config script.
LIBS_PLAIN:=SDL_image GL z
# Libraries that have a lib-config script.
LIBS_CONFIG:=xml2 sdl libpng

