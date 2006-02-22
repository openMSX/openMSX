# $Id$
#
# Configuration for Darwin.

# Does platform support symlinks?
USE_SYMLINK:=true

# Default compiler.
OPENMSX_CXX?=g++

# File name extension of executables.
EXEEXT:=

# Bind when executable is loaded, rather then when symbols are accessed.
# I don't know why, but the linker suggests this.
LINK_FLAGS+=-bind_at_load

# Probe Overrides
# ===============

DIR_IF_EXISTS=$(shell test -d $(1) && echo $(1))

# DarwinPorts library and header paths.
DARWINPORTS_CFLAGS:=$(addprefix -I,$(call DIR_IF_EXISTS,/opt/local/include))
DARWINPORTS_LDFLAGS:=$(addprefix -L,$(call DIR_IF_EXISTS,/opt/local/lib))

# Fink library and header paths.
FINK_CFLAGS:=$(addprefix -I,$(call DIR_IF_EXISTS,/sw/include))
FINK_LDFLAGS:=$(addprefix -L,$(call DIR_IF_EXISTS,/sw/lib))

MMAP_PREHEADER:=<sys/types.h>
SYS_MMAN_PREHEADER:=<sys/types.h>
SYS_SOCKET_PREHEADER:=<sys/types.h>
# TODO:
# GL_HEADER:=<OpenGL/gl.h> iso GL_CFLAGS is cleaner,
# but we have to modify the build before we can use it.
GL_CFLAGS:=-I/System/Library/Frameworks/OpenGL.framework/Headers
GL_LDFLAGS:=-framework OpenGL -lGL \
	-L/System/Library/Frameworks/OpenGL.framework/Libraries

GLEW_CFLAGS+=$(DARWINPORTS_CFLAGS) $(FINK_CFLAGS)
GL_GLEW_CFLAGS+=$(DARWINPORTS_CFLAGS) $(FINK_CFLAGS)
GLEW_LDFLAGS+=$(DARWINPORTS_LDFLAGS) $(FINK_LDFLAGS)
