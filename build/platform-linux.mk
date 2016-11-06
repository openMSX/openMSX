# Configuration for Linux machines.

# Does platform support symlinks?
USE_SYMLINK:=true

# File name extension of executables.
EXEEXT:=
LIBRARYEXT:=.so

# In glibc the function clock_gettime() is defined in the librt library. In
# uClibc it's defined in libc itself. We should write a function that actually
# tests which library defines it. But as a temporary workaround/hack we figure
# out the linker flags based on the occurrence of the substring uclibc inside
# the compiler executable name (e.g. for Dingoo we don't need to link against
# librt).
ifeq (,$(findstring uclibc,$(CXX)))
	LINK_FLAGS+=-lrt
endif
