# $Id$

DIR_IF_EXISTS=$(shell test -d $(1) && echo $(1))

# MacPorts library and header paths.
MACPORTS_CFLAGS:=$(addprefix -I,$(call DIR_IF_EXISTS,/opt/local/include))
MACPORTS_LDFLAGS:=$(addprefix -L,$(call DIR_IF_EXISTS,/opt/local/lib))

# Fink library and header paths.
FINK_CFLAGS:=$(addprefix -I,$(call DIR_IF_EXISTS,/sw/include))
FINK_LDFLAGS:=$(addprefix -L,$(call DIR_IF_EXISTS,/sw/lib))

MMAP_PREHEADER:=<sys/types.h>
# TODO:
# GL_HEADER:=<OpenGL/gl.h> iso GL_CFLAGS is cleaner,
# but we have to modify the build before we can use it.
GL_CFLAGS:=-I/System/Library/Frameworks/OpenGL.framework/Headers
GL_LDFLAGS:=-framework OpenGL -lGL \
	-L/System/Library/Frameworks/OpenGL.framework/Libraries

GLEW_CFLAGS_SYS_DYN+=$(MACPORTS_CFLAGS) $(FINK_CFLAGS)
GLEW_LDFLAGS_SYS_DYN+=$(MACPORTS_LDFLAGS) $(FINK_LDFLAGS)

JACK_CFLAGS_SYS_DYN+=$(MACPORTS_CFLAGS) $(FINK_CFLAGS)
JACK_LDFLAGS_SYS_DYN+=$(MACPORTS_LDFLAGS) $(FINK_LDFLAGS)

SDL_LDFLAGS_3RD_STA:=`$(3RDPARTY_INSTALL_DIR)/bin/sdl-config --static-libs`

ifneq ($(filter 3RD_%,$(LINK_MODE)),)
# Use tclConfig.sh from /usr: ideally we would use tclConfig.sh from the SDK,
# but the SDK doesn't contain that file. The -isysroot compiler argument makes
# sure the headers are taken from the SDK though.
TCL_CFLAGS_SYS_DYN:=`build/tcl-search.sh --cflags`
TCL_LDFLAGS_SYS_DYN:=`build/tcl-search.sh --ldflags`

# Use xml2-config from /usr: ideally we would use xml2-config from the SDK,
# but the SDK doesn't contain that file. The -isysroot compiler argument makes
# sure the headers are taken from the SDK though.
XML_CFLAGS_SYS_DYN:=`/usr/bin/xml2-config --cflags`
XML_LDFLAGS_SYS_DYN:=`/usr/bin/xml2-config --libs`
XML_RESULT_SYS_DYN:=`/usr/bin/xml2-config --version`
endif
