# $Id$
#
# Configuration for MinGW on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=false

# Default build flavour.
OPENMSX_FLAVOUR?=uow32

# Default compiler.
OPENMSX_CXX?=g++ -B/mingw

# File name extension of executables.
EXEEXT:=.exe

# Libraries that do not have a lib-config script.
LIBS_PLAIN:=stdc++ SDL_image z
# Libraries that have a lib-config script.
LIBS_CONFIG:=xml2 sdl

# Compiler flags.
CXXFLAGS+= \
	-mthreads -mconsole -mms-bitfields \
	-D__GTHREAD_HIDE_WIN32API \
	-DNO_X11 -DNO_MMAP -DNO_SOCKET -DNO_BZERO -DNO_LINUX_RTC \
	-DFS_CASEINSENSE \
	-DUOW32
# TODO: Not sure this is needed anymore if "g++ -B" is used.
#	-I/mingw/include -I/mingw/include/w32api

# Linker flags.
LINK_FLAGS:=-L/mingw/lib -L/mingw/lib/w32api $(LINK_FLAGS)

