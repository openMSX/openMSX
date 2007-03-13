# $Id$
#
# Configuration for creating a Darwin app folder.
# In practice, this is used for Mac OS X; I'm not sure all of it applies to
# other Darwin-based systems.
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
# - libjpeg (used by SDL_image)
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

# Disable Jack: there is an OS X version (jackosx.com), but since we do not
# include the Jack framework in the app folder, the executable should not be
# linked to it.
DISABLED_HEADERS+=JACK_H
DISABLED_LIBS+=JACK

# GLEW header can be <GL/glew.h> or just <glew.h>; the dedicated version we use
# resides in the "GL" dir, so don't look for the other one, or we might pick
# up a different version somewhere on the system.
DISABLED_HEADERS+=GLEW_H

# Probe Overrides
# ===============

# Assume all static libraries come from the same distribution (such as Fink or
# DarwinPorts); use SDL's config script to find the rest.
SDL_CONFIG_PATH:=$(shell PATH=$(PATH) ; which sdl-config)
ifneq ($(shell test -x $(SDL_CONFIG_PATH) ; echo $$?),0)
$(error Cannot execute sdl-config)
endif

STATIC_LIBS_DIR:=$(shell $(SDL_CONFIG_PATH) --static-libs | sed -e "s%^\(.* \)*\(/.*\)/libSDL\.a.*$$%\2%" 2>> /dev/null)

PNG_LDFLAGS:=$(STATIC_LIBS_DIR)/libpng.a
# TODO: In theory this should return the flags for static linking,
#       but in libpng 1.2.8 it just returns the dynamic link flags.
#PNG_LDFLAGS:=`libpng-config --static --libs 2>> $(LOG)`

SDL_LDFLAGS:=`$(SDL_CONFIG_PATH) --static-libs | sed -e \"s/-L[^ ]*//g\" 2>> $(LOG)`

# Note: Depending on how SDL_image is compiled, it may or may not need libjpeg.
SDL_IMAGE_LDFLAGS:=$(SDL_LDFLAGS) $(PNG_LDFLAGS) \
	$(STATIC_LIBS_DIR)/libSDL_image.a \
	$(shell grep -q "\-ljpeg" $(STATIC_LIBS_DIR)/libSDL_image.la 2>> /dev/null \
		&& echo "$(STATIC_LIBS_DIR)/libjpeg.a")

GLEW_CFLAGS:=-I$(STATIC_LIBS_DIR)/../include
GLEW_LDFLAGS:=$(STATIC_LIBS_DIR)/libGLEW.a
