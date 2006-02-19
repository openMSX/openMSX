# $Id$
#
# Configuration for MinGW on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=false

# Default compiler.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=.exe

# Compiler flags.
CXXFLAGS+= \
	-mthreads -mconsole -mms-bitfields \
	-I/mingw/include -I/mingw/include/w32api\
	`if test -d /usr/local/include; then echo '-I/usr/local/include -I/usr/local/include/directx'; fi` \
	-D__GTHREAD_HIDE_WIN32API \
	-DFS_CASEINSENSE

# Linker flags.
LINK_FLAGS:=-L/mingw/lib -L/mingw/lib/w32api -lwsock32 -ldsound \
	`if test -d /usr/local/lib; then echo '-L/usr/local/lib'; fi` \
	$(LINK_FLAGS)


# Probe Overrides
# ===============

PNG_CFLAGS:=

PNG_LDFLAGS:=-lpng
PNG_RESULT:=yes

GL_LDFLAGS:=-lopengl32

GLEW_LDFLAGS:=-lglew32
