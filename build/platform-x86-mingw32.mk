# $Id$
#
# Configuration for MinGW on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=false

# Default build flavour.
OPENMSX_FLAVOUR?=win32

# Default compiler.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=.exe

# Libraries that do not have a lib-config script.
#LIBS_PLAIN:=stdc++ SDL_image png z tcl84 winmm
LIBS_PLAIN:=stdc++ SDL_image png z tcl84 winmm opengl32
# Libraries that have a lib-config script.
LIBS_CONFIG:=xml2 sdl

# Compiler flags.
CXXFLAGS+= \
	-mthreads -mconsole -mms-bitfields \
	-I/mingw/include -I/mingw/include/w32api \
	-D__GTHREAD_HIDE_WIN32API \
	-DFS_CASEINSENSE

# Linker flags.
LINK_FLAGS:=-L/mingw/lib -L/mingw/lib/w32api $(LINK_FLAGS)

