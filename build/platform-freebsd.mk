# Configuration for FreeBSD.

# Does platform support symlinks?
USE_SYMLINK:=true

# File name extension of executables.
EXEEXT:=
LIBRARYEXT:=.so

COMPILE_FLAGS+=-D_REENTRANT -D_THREAD_SAFE

LINK_FLAGS:= -static-libgcc -static-libstdc++ $(LINK_FLAGS)
