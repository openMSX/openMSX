# $Id$
#
# Configuration for creating a Darwin app folder.
# In practice, this is used for Mac OS X; I'm not sure all of it applies to
# other Darwin-based systems.
#
# TODO:
# This Makefile does two things:
# - sets SET_WINDOW_ICON to false, so app folder icon is used
# - sets flags for linking non-platform libs statically
# However, these should be two independent choices:
# - executable intended as standalone vs intended for use in app folder
# - link non-platform libs statically vs dynamically
#
# The following libraries are standard on Mac OS X:
# - libxml2
# - OpenGL
# - TCL
# - zlib
# The following libraries will be linked statically:
# - libpng
# - SDL
# - SDL_image
# - GLEW
# No libraries will be included in the app folder.
# The following libraries will be disabled:
# - Jack

include build/platform-darwin.mk

# Enable prebinding for better startup performance.
# On OS X 10.3 the system frameworks are using overlapping addresses, so
# prebinding fails. Let's try again later.
#LINK_FLAGS+=-prebind

# The app folder will set a hi-res icon, so the openMSX process should not
# replace this with its own low-res icon.
SET_WINDOW_ICON:=false

# Disable Jack: there is an OS X version (jackosx.com), but we do not want to
# use it in the binary distribution of openMSX.
DISABLED_LIBRARIES+=JACK

# GLEW header can be <GL/glew.h> or just <glew.h>; the dedicated version we use
# resides in the "GL" dir, so don't look for the other one, or we might pick
# up a different version somewhere on the system.
DISABLED_HEADERS+=GLEW_H

# Select the SDK for the OS X version we want to be compatible with.
ifeq ($(OPENMSX_TARGET_CPU),ppc)
SDK_PATH:=$(firstword $(sort $(wildcard /Developer/SDKs/MacOSX10.3.?.sdk)))
OPENMSX_CXX:=g++-3.3
else
SDK_PATH:=/Developer/SDKs/MacOSX10.4u.sdk
endif
COMPILE_ENV+=NEXT_ROOT=$(SDK_PATH)
LINK_ENV+=NEXT_ROOT=$(SDK_PATH)


# Probe Overrides
# ===============

ifneq ($(OPENMSX_TARGET_CPU),univ)

# Assume all static libraries come from the same distribution (such as Fink or
# DarwinPorts); use SDL's config script to find the rest.
ifeq ($(STATIC_INSTALL_DIR),)
STATIC_INSTALL_DIR:=$(shell sdl-config --static-libs | sed -e "s%^\(.* \)*\(/.*\)/lib/libSDL\.a.*$$%\2%" 2>> /dev/null)
else
# Static install dir was passed explicitly; check that it contains sdl-config.
# Otherwise, we might pick up systemwide libraries in the probe.
ifneq ($(shell test -x $(STATIC_INSTALL_DIR)/bin/sdl-config ; echo $$?),0)
$(error Cannot execute sdl-config)
endif
endif

PNG_CFLAGS:=`$(STATIC_INSTALL_DIR)/bin/libpng-config --cflags 2>> $(LOG)`
PNG_LDFLAGS:=$(STATIC_INSTALL_DIR)/lib/libpng.a
# TODO: In theory this should return the flags for static linking,
#       but in libpng 1.2.8 it just returns the dynamic link flags.
#PNG_LDFLAGS:=`$(STATIC_INSTALL_DIR)/bin/libpng-config --static --libs 2>> $(LOG)`
PNG_RESULT:=`$(STATIC_INSTALL_DIR)/bin/libpng-config --version`

SDL_CFLAGS:=`$(STATIC_INSTALL_DIR)/bin/sdl-config --cflags 2>> $(LOG)`
SDL_LDFLAGS:=`$(STATIC_INSTALL_DIR)/bin/sdl-config --static-libs | sed -e \"s/-L[^ ]*//g\" 2>> $(LOG)`
SDL_RESULT:=`$(STATIC_INSTALL_DIR)/bin/sdl-config --version`

SDL_IMAGE_CFLAGS:=$(SDL_CFLAGS)
# Note: Depending on how SDL_image is compiled, it may or may not need libjpeg.
SDL_IMAGE_LDFLAGS:=$(SDL_LDFLAGS) $(PNG_LDFLAGS) \
	$(STATIC_INSTALL_DIR)/lib/libSDL_image.a \
	$(shell grep -q "\-ljpeg" $(STATIC_INSTALL_DIR)/lib/libSDL_image.la 2>> /dev/null \
		&& echo "$(STATIC_INSTALL_DIR)/lib/libjpeg.a")

GLEW_CFLAGS:=-I$(STATIC_INSTALL_DIR)/include
GLEW_LDFLAGS:=$(STATIC_INSTALL_DIR)/lib/libGLEW.a

endif
