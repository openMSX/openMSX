# $Id$
#
# Replacement for "configure".
# Performs some test compiles, to check for headers and functions.
# Unlike configure, it does not run any test code, so it is more friendly for
# cross compiles.
#
# Some notes about static linking:
# There are two ways of linking to static library: using the -l command line
# option or specifying the full path to the library file as one of the inputs.
# When using the -l option, the library search paths will be searched for a
# dynamic version of the library, if that is not found, the search paths will
# be searched for a static version of the library. This means we cannot force
# static linking of a library this way. It is possible to force static linking
# of all libraries, but we want to control it per library.
# Conclusion: We have to specify the full path to each library that should be
#             linked statically.
#
# Legend of the suffixes:
# _SYS_DYN: link dynamically against system libs
#           this is the default mode; useful for local binaries
# _SYS_STA: link statically against system libs
#           this seems pointless to me; not implemented
# _3RD_DYN: link dynamically against libs from non-system dir
#           might be useful; not implemented
# _3RD_STA: link statically against libs from non-system dir
#           this is how we build our redistributable binaries

# This Makefile needs parameters to operate; check that they were specified:
# - output directory
ifeq ($(OUTDIR),)
$(error Missing parameter: OUTDIR)
endif
# - command line to invoke compiler + options
ifeq ($(COMPILE),)
$(error Missing parameter: COMPILE)
endif
# - OS we're building for
ifeq ($(OPENMSX_TARGET_OS),)
$(error Missing parameter: OPENMSX_TARGET_OS)
endif

# Result files.
LOG:=$(OUTDIR)/probe.log
OUTHEADER:=$(OUTDIR)/probed_defs.hh
OUTMAKE:=$(OUTDIR)/probed_defs.mk

# Functions
# =========

ALL_FUNCS:=FTRUNCATE GETTIMEOFDAY MMAP USLEEP POSIX_MEMALIGN

FTRUNCATE_FUNC:=ftruncate
FTRUNCATE_HEADER:=<unistd.h>

GETTIMEOFDAY_FUNC:=gettimeofday
GETTIMEOFDAY_HEADER:=<sys/time.h>

MMAP_FUNC:=mmap
MMAP_HEADER:=<sys/mman.h>

USLEEP_FUNC:=usleep
USLEEP_HEADER:=<unistd.h>

POSIX_MEMALIGN_FUNC:=posix_memalign
POSIX_MEMALIGN_HEADER:=<stdlib.h>

# Disabled X11, because it is not useful yet and the link flags are not here.
#X11_FUNC:=XtMalloc
#X11_HEADER:=<X11/Intrinsic.h>


# Headers
# =======

ALL_HEADERS:=$(addsuffix _H, \
	GL GL_GL GLEW GL_GLEW JACK PNG SDL SDL_IMAGE SDL_TTF \
	SYS_MMAN SYS_SOCKET TCL XML ZLIB )

# Location of GL headers is not standardised; if one of these matches,
# we consider the GL headers found.
GL_HEADER:=<gl.h>
GL_GL_HEADER:=<GL/gl.h>
GL_CFLAGS:=
# Use GL_CFLAGS for GL_GL as well, if someone overrides it.
# In any case, only GL_CFLAGS will be used by the actual build.
GL_GL_CFLAGS=$(GL_CFLAGS)

# The comment for the GL headers applies to GLEW as well.
GLEW_HEADER:=<glew.h>
GL_GLEW_HEADER:=<GL/glew.h>
GLEW_CFLAGS_SYS_DYN:=
GLEW_CFLAGS_3RD_STA:=-I$(3RDPARTY_INSTALL_DIR)/include

JACK_HEADER:=<jack/jack.h>
JACK_CFLAGS_SYS_DYN:=
JACK_CFLAGS_3RD_STA:=-I$(3RDPARTY_INSTALL_DIR)/include

PNG_HEADER:=<png.h>
PNG_CFLAGS_SYS_DYN:=`libpng-config --cflags 2>> $(LOG)`
# Note: The additional -I is to pick up the zlib headers when zlib is not
#       installed systemwide.
PNG_CFLAGS_3RD_STA:=`$(3RDPARTY_INSTALL_DIR)/bin/libpng-config --cflags 2>> $(LOG)` -I$(3RDPARTY_INSTALL_DIR)/include

SDL_HEADER:=<SDL.h>
SDL_CFLAGS_SYS_DYN:=`sdl-config --cflags 2>> $(LOG)`
SDL_CFLAGS_3RD_STA:=`$(3RDPARTY_INSTALL_DIR)/bin/sdl-config --cflags 2>> $(LOG)`

SDL_IMAGE_HEADER:=<SDL_image.h>
# Note: "=" instead of ":=", so overriden value of SDL_CFLAGS will be used.
SDL_IMAGE_CFLAGS_SYS_DYN=$(SDL_CFLAGS_SYS_DYN)
SDL_IMAGE_CFLAGS_3RD_STA=$(SDL_CFLAGS_3RD_STA)

SDL_TTF_HEADER:=<SDL_ttf.h>
# Note: "=" instead of ":=", so overriden value of SDL_CFLAGS will be used.
SDL_TTF_CFLAGS_SYS_DYN=$(SDL_CFLAGS_SYS_DYN)
SDL_TTF_CFLAGS_3RD_STA=$(SDL_CFLAGS_3RD_STA)

SYS_MMAN_HEADER:=<sys/mman.h>

SYS_SOCKET_HEADER:=<sys/socket.h>

TCL_HEADER:=<tcl.h>
TCL_CFLAGS_SYS_DYN:=`build/tcl-search.sh --cflags 2>> $(LOG)`
TCL_CFLAGS_3RD_STA:=`TCL_CONFIG_DIR=$(3RDPARTY_INSTALL_DIR)/lib build/tcl-search.sh --cflags 2>> $(LOG)`

XML_HEADER:=<libxml/parser.h>
XML_CFLAGS_SYS_DYN:=`xml2-config --cflags 2>> $(LOG)`
XML_CFLAGS_3RD_STA:=`$(3RDPARTY_INSTALL_DIR)/bin/xml2-config --cflags 2>> $(LOG)` -DLIBXML_STATIC

ZLIB_HEADER:=<zlib.h>
ZLIB_CFLAGS_SYS_DYN:=
ZLIB_CFLAGS_3RD_STA:=-I$(3RDPARTY_INSTALL_DIR)/include


# Libraries
# =========

ALL_LIBS:=GL GLEW JACK PNG SDL SDL_IMAGE SDL_TTF TCL XML ZLIB
ALL_LIBS+=ABC XYZ

GL_LDFLAGS:=-lGL
GL_RESULT:=yes

GLEW_LDFLAGS_SYS_DYN:=-lGLEW
GLEW_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libGLEW.a
GLEW_RESULT:=yes

JACK_LDFLAGS_SYS_DYN:=-ljack
JACK_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libjack.a
JACK_RESULT:=yes

PNG_LDFLAGS_SYS_DYN:=`libpng-config --ldflags 2>> $(LOG)`
PNG_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libpng12.a
PNG_RESULT_SYS_DYN:=`libpng-config --version`
PNG_RESULT_3RD_STA:=`$(3RDPARTY_INSTALL_DIR)/bin/libpng-config --version`

SDL_LDFLAGS_SYS_DYN:=`sdl-config --libs 2>> $(LOG)`
SDL_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libSDL.a
SDL_RESULT_SYS_DYN:=`sdl-config --version`
SDL_RESULT_3RD_STA:=`$(3RDPARTY_INSTALL_DIR)/bin/sdl-config --version`

# Note: "=" instead of ":=", so overriden value of SDL_LDFLAGS will be used.
SDL_IMAGE_LDFLAGS_SYS_DYN=-lSDL_image $(SDL_LDFLAGS_SYS_DYN)
SDL_IMAGE_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libSDL_image.a
SDL_IMAGE_RESULT:=yes

SDL_TTF_LDFLAGS_SYS_DYN=-lSDL_ttf $(SDL_LDFLAGS_SYS_DYN)
SDL_TTF_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libSDL_ttf.a $(3RDPARTY_INSTALL_DIR)/lib/libfreetype.a
SDL_TTF_RESULT:=yes

TCL_LDFLAGS_SYS_DYN:=`build/tcl-search.sh --ldflags 2>> $(LOG)`
# Note: Tcl can be compiled with a static or a dynamic library, not both.
#       So whether this returns static or dynamic link flags depends on how
#       this copy of Tcl was built.
TCL_LDFLAGS_3RD_STA:=`TCL_CONFIG_DIR=$(3RDPARTY_INSTALL_DIR)/lib build/tcl-search.sh --static-libs 2>> $(LOG)`
TCL_RESULT_SYS_DYN:=`build/tcl-search.sh --version 2>> $(LOG)`
TCL_RESULT_3RD_STA:=`TCL_CONFIG_DIR=$(3RDPARTY_INSTALL_DIR)/lib build/tcl-search.sh --version 2>> $(LOG)`

XML_LDFLAGS_SYS_DYN:=`xml2-config --libs 2>> $(LOG)`
XML_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libxml2.a
XML_RESULT_SYS_DYN:=`xml2-config --version`
XML_RESULT_3RD_STA:=`$(3RDPARTY_INSTALL_DIR)/bin/xml2-config --version`

ZLIB_LDFLAGS_SYS_DYN:=-lz
ZLIB_LDFLAGS_3RD_STA:=$(3RDPARTY_INSTALL_DIR)/lib/libz.a
ZLIB_RESULT:=yes

# Libraries that do not exist:
# (these are to test error reporting, it is expected that they are not found)

ABC_LDFLAGS:=`abc-config --libs 2>> $(LOG)`
ABC_RESULT:=impossible

XYZ_LDFLAGS:=-lxyz
XYZ_RESULT:=impossible


# OS Specific
# ===========

DISABLED_FUNCS:=
DISABLED_LIBS:=
DISABLED_HEADERS:=

SYSTEM_LIBS:=

# Initial value of DISABLED_LIBRARIES is set here.
include build/custom.mk

ifneq ($(filter 3RD_%,$(LINK_MODE)),)
# Disable Jack: The CassetteJack feature is not useful for most end users, so
# do not include Jack in the binary distribution of openMSX.
DISABLED_LIBRARIES+=JACK

# GLEW header can be <GL/glew.h> or just <glew.h>; the dedicated version we use
# resides in the "GL" dir, so don't look for the other one, or we might pick
# up a different version somewhere on the system.
DISABLED_HEADERS+=GLEW_H
endif

# Allow the OS specific Makefile to override if necessary.
include build/platform-$(OPENMSX_TARGET_OS).mk

DISABLED_HEADERS+=$(addsuffix _H,$(DISABLED_LIBRARIES))
DISABLED_LIBS+=$(DISABLED_LIBRARIES)


# Implementation
# ==============

# Resolve probe strings depending on link mode and list of system libs.

DEFCHECKGET=$(strip \
	$(if $(filter _undefined,_$(origin $(1))), \
		$(error Variable $(1) is undefined) ) \
	)$($(1))
RESOLVEMODE=$(call DEFCHECKGET,$(1)_$(2)_$(if $(filter $(1),$(SYSTEM_LIBS)),SYS_DYN,$(LINK_MODE)))

GLEW_CFLAGS:=$(call RESOLVEMODE,GLEW,CFLAGS)
GLEW_LDFLAGS:=$(call RESOLVEMODE,GLEW,LDFLAGS)
GL_GLEW_CFLAGS:=$(GLEW_CFLAGS)

JACK_CFLAGS:=$(call RESOLVEMODE,JACK,CFLAGS)
JACK_LDFLAGS:=$(call RESOLVEMODE,JACK,LDFLAGS)

PNG_CFLAGS:=$(call RESOLVEMODE,PNG,CFLAGS)
PNG_LDFLAGS:=$(call RESOLVEMODE,PNG,LDFLAGS)
PNG_RESULT:=$(call RESOLVEMODE,PNG,RESULT)

SDL_CFLAGS:=$(call RESOLVEMODE,SDL,CFLAGS)
SDL_LDFLAGS:=$(call RESOLVEMODE,SDL,LDFLAGS)
SDL_RESULT:=$(call RESOLVEMODE,SDL,RESULT)

SDL_IMAGE_CFLAGS:=$(call RESOLVEMODE,SDL_IMAGE,CFLAGS)
SDL_IMAGE_LDFLAGS:=$(call RESOLVEMODE,SDL_IMAGE,LDFLAGS)

SDL_TTF_CFLAGS:=$(call RESOLVEMODE,SDL_TTF,CFLAGS)
SDL_TTF_LDFLAGS:=$(call RESOLVEMODE,SDL_TTF,LDFLAGS)

TCL_CFLAGS:=$(call RESOLVEMODE,TCL,CFLAGS)
TCL_LDFLAGS:=$(call RESOLVEMODE,TCL,LDFLAGS)
TCL_RESULT:=$(call RESOLVEMODE,TCL,RESULT)

XML_CFLAGS:=$(call RESOLVEMODE,XML,CFLAGS)
XML_LDFLAGS:=$(call RESOLVEMODE,XML,LDFLAGS)
XML_RESULT:=$(call RESOLVEMODE,XML,RESULT)

ZLIB_CFLAGS:=$(call RESOLVEMODE,ZLIB,CFLAGS)
ZLIB_LDFLAGS:=$(call RESOLVEMODE,ZLIB,LDFLAGS)

# Determine which probes to run.

CHECK_FUNCS:=$(filter-out $(DISABLED_FUNCS),$(ALL_FUNCS))
CHECK_HEADERS:=$(filter-out $(DISABLED_HEADERS),$(ALL_HEADERS))
CHECK_LIBS:=$(filter-out $(DISABLED_LIBS),$(ALL_LIBS))

CHECK_TARGETS:=hello $(ALL_FUNCS) $(ALL_HEADERS) $(ALL_LIBS)
PRINT_LIBS:=$(addsuffix -print,$(CHECK_LIBS))

.PHONY: all init check-targets print-libs $(CHECK_TARGETS) $(PRINT_LIBS)

# Default target.
all: check-targets print-libs
	@$(PYTHON) build/probe-header.py $(OUTMAKE) > $(OUTHEADER)

check-targets: $(CHECK_TARGETS)
print-libs: $(PRINT_LIBS)

# Create empty log and result files.
init:
	@echo "Probing target system..."
	@mkdir -p $(OUTDIR)
	@echo "Probing system:" > $(LOG)
	@echo "# Automatically generated by build system." > $(OUTMAKE)
	@echo "# Non-empty value means found, empty means not found." >> $(OUTMAKE)
	@echo "PROBE_MAKE_INCLUDED:=true" >> $(OUTMAKE)
	@echo "DISABLED_FUNCS:=$(DISABLED_FUNCS)" >> $(OUTMAKE)
	@echo "DISABLED_LIBS:=$(DISABLED_LIBS)" >> $(OUTMAKE)
	@echo "DISABLED_HEADERS:=$(DISABLED_HEADERS)" >> $(OUTMAKE)
	@echo "HAVE_X11:=" >> $(OUTMAKE)

# Check compiler with the most famous program.
hello: init
	@echo "#include <iostream>" > $(OUTDIR)/$@.cc
	@echo "int main(int argc, char** argv) {" >> $(OUTDIR)/$@.cc
	@echo "  std::cout << \"Hello World!\" << std::endl;" >> $(OUTDIR)/$@.cc
	@echo "}" >> $(OUTDIR)/$@.cc
	@if $(COMPILE) $(COMPILE_FLAGS) -c $(OUTDIR)/$@.cc -o $(OUTDIR)/$@.o 2>> $(LOG); \
	then echo "Compiler works: $(COMPILE) $(COMPILE_FLAGS)" >> $(LOG); \
	     echo "COMPILER:=true" >> $(OUTMAKE); \
	else echo "Compiler broken: $(COMPILE) $(COMPILE_FLAGS)" >> $(LOG); \
	     echo "COMPILER:=false" >> $(OUTMAKE); \
	fi
	@rm -f $(OUTDIR)/$@.cc $(OUTDIR)/$@.o

# Probe for function:
# Try to include the necessary header and get the function address.
$(CHECK_FUNCS): init
	@echo > $(OUTDIR)/$@.cc
	@if [ -n "$($@_PREHEADER)" ]; then echo "#include $($@_PREHEADER)"; fi \
		>> $(OUTDIR)/$@.cc
	@echo "#include $($@_HEADER)" >> $(OUTDIR)/$@.cc
	@echo "void (*f)() = reinterpret_cast<void (*)()>($($@_FUNC));" \
		>> $(OUTDIR)/$@.cc
	@if $(COMPILE) $(COMPILE_FLAGS) -c $(OUTDIR)/$@.cc -o $(OUTDIR)/$@.o \
		2>> $(LOG); \
	then echo "Found function: $@" >> $(LOG); \
	     echo "HAVE_$@:=true" >> $(OUTMAKE); \
	else echo "Missing function: $@" >> $(LOG); \
	     echo "HAVE_$@:=" >> $(OUTMAKE); \
	fi
	@rm -f $(OUTDIR)/$@.cc $(OUTDIR)/$@.o

$(DISABLED_FUNCS): init
	@echo "Disabled function: $@" >> $(LOG)
	@echo "HAVE_$@:=" >> $(OUTMAKE)

# Probe for header:
# Try to include the header.
$(CHECK_HEADERS): init
	@echo > $(OUTDIR)/$@.cc
	@if [ -n "$($(@:%_H=%)_PREHEADER)" ]; then \
		echo "#include $($(@:%_H=%)_PREHEADER)"; fi >> $(OUTDIR)/$@.cc
	@echo "#include $($(@:%_H=%)_HEADER)" >> $(OUTDIR)/$@.cc
	@if FLAGS="$($(@:%_H=%_CFLAGS))" && eval $(COMPILE) \
		$(COMPILE_FLAGS) "$$FLAGS" \
		-c $(OUTDIR)/$@.cc -o $(OUTDIR)/$@.o 2>> $(LOG); \
	then echo "Found header: $(@:%_H=%)" >> $(LOG); \
	     echo "HAVE_$@:=true" >> $(OUTMAKE); \
	else echo "Missing header: $(@:%_H=%)" >> $(LOG); \
	     echo "HAVE_$@:=" >> $(OUTMAKE); \
	fi
	@rm -f $(OUTDIR)/$@.cc $(OUTDIR)/$@.o

$(DISABLED_HEADERS): init
	@echo "Disabled header: $(@:%_H=%)" >> $(LOG)
	@echo "HAVE_$@:=" >> $(OUTMAKE)

# Probe for library:
# Try to link dummy program to the library.
$(CHECK_LIBS): init
	@echo "int main(int argc, char **argv) { return 0; }" > $(OUTDIR)/$@.cc
	@if FLAGS="$($@_LDFLAGS)" && eval $(COMPILE) \
		$(OUTDIR)/$@.cc -o $(OUTDIR)/$@.exe \
		$(LINK_FLAGS) "$$FLAGS" 2>> $(LOG); \
	then echo "Found library: $@" >> $(LOG); \
	     echo "HAVE_$@_LIB:=$($@_RESULT)" >> $(OUTMAKE); \
	else echo "Missing library: $@" >> $(LOG); \
	     echo "HAVE_$@_LIB:=" >> $(OUTMAKE); \
	fi
	@rm -f $(OUTDIR)/$@.cc $(OUTDIR)/$@.exe

$(DISABLED_LIBS): init
	@echo "Disabled library: $@" >> $(LOG)
	@echo "HAVE_$@_LIB:=" >> $(OUTMAKE)

# Print the flags for using a certain library (CFLAGS and LDFLAGS).
$(PRINT_LIBS): check-targets
	@echo "$(@:%-print=%)_CFLAGS:=$($(@:%-print=%)_CFLAGS)" >> $(OUTMAKE)
	@echo "$(@:%-print=%)_LDFLAGS:=$($(@:%-print=%)_LDFLAGS)" >> $(OUTMAKE)

