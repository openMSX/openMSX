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

# Libraries that do not have a lib-config script.
LIBS_PLAIN:=stdc++ SDL_image z
# Libraries that have a lib-config script.
LIBS_CONFIG:=xml2 sdl

