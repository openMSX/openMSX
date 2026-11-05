# Configuration for FreeBSD.

# Does platform support symlinks?
USE_SYMLINK:=true

# File name extension of executables.
EXEEXT:=
LIBRARYEXT:=.so

COMPILE_FLAGS+=-D_REENTRANT -D_THREAD_SAFE

LINK_FLAGS:= -static-libgcc -static-libstdc++ $(LINK_FLAGS)

# Don't let CMake use precompiled headers for the 3rd party libraries: with
# GCC on FreeBSD, compiling SDL3 fails with "sorry, unimplemented: PCH
# allocation failure".
CMAKE_TARGET_FLAGS+=-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
