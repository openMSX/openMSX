# $Id$
#
# Configuration for MinGW on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=false

# File name extension of executables.
EXEEXT:=.exe

# Compiler flags.
COMPILE_FLAGS+= \
	-mthreads -mms-bitfields \
	-I/mingw/include -I/mingw/include/w32api\
	`if test -d /usr/local/include; then echo '-I/usr/local/include -I/usr/local/include/directx'; fi` \
	-D__GTHREAD_HIDE_WIN32API \
	-DFS_CASEINSENSE

# Linker flags.
LINK_FLAGS:=-L/mingw/lib -L/mingw/lib/w32api -lwsock32 -lwinmm -ldsound -lsecur32 \
	`if test -d /usr/local/lib; then echo '-L/usr/local/lib'; fi` \
	-mconsole \
	$(LINK_FLAGS)
