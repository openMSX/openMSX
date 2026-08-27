# Configuration for MinGW on x86 machines.

# Does platform support symlinks?
USE_SYMLINK:=false

# File name extension of executables.
EXEEXT:=.exe
LIBRARYEXT:=.dll

# Compiler flags.
COMPILE_FLAGS+= \
	-mthreads -mms-bitfields \
	-I/mingw/include -I/mingw/include/w32api \
	-DWIN32_LEAN_AND_MEAN -D__GTHREAD_HIDE_WIN32API \
	-DFS_CASEINSENSE

# Linker flags.
LINK_FLAGS:= \
	-L/mingw/lib -L/mingw/lib/w32api -lws2_32 -lwsock32 -lwinmm -ldsound -lsecur32 -liphlpapi -lcrypt32 \
	 -Wl,-subsystem,windows -static-libgcc -static-libstdc++ \
	$(LINK_FLAGS)
