# $Id$
#
# Configuration for FreeBSD 4.x on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default build flavour.
OPENMSX_FLAVOUR?=i686

# Default compiler.
OPENMSX_CXX?=g++33

# File name extension of executables.
EXEEXT:=

CXXFLAGS+=-D_REENTRANT -D_THREAD_SAFE \
	`if [ -d /usr/X11R6/include ]; then echo '-I/usr/X11R6/include'; fi` \
	`if [ -d /usr/local/include ]; then echo '-I/usr/local/include'; fi` 

LINK_FLAGS+=-pthread \
	`if [ -d /usr/X11R6/lib ]; then echo '-L/usr/X11R6/lib'; fi` \
	`if [ -d /usr/local/lib ]; then echo '-L/usr/local/lib'; fi` 


# Probe Overrides
# ===============

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>

SDL_CFLAGS:=`sdl11-config --cflags 2>> $(LOG)`

SDL_LDFLAGS:=`sdl11-config --libs 2>> $(LOG)`
SDL_RESULT:=`sdl11-config --version`

PNG_CFLAGS:=

PNG_LDFLAGS:=-lpng
PNG_RESULT:=yes
