# $Id$
# Defines the building blocks of openMSX and their dependencies.

ifneq ($(PROBE_MAKE_INCLUDED),true)
$(error Include probe results before including "components.mk")
endif

# For static linking, it's important that if lib A depends on B, A is in the
# list before B.
# TODO: Would it be better to use different LDFLAGS instead?
CORE_LIBS:=SDL_IMAGE SDL_TTF SDL PNG TCL XML ZLIB
ifneq ($(filter x,$(foreach LIB,$(CORE_LIBS),x$(HAVE_$(LIB)_LIB))),)
COMPONENT_CORE:=false
endif
ifneq ($(filter x,$(foreach LIB,$(CORE_LIBS),x$(HAVE_$(LIB)_H))),)
COMPONENT_CORE:=false
endif
COMPONENT_CORE?=true

ifeq ($(HAVE_GL_LIB),)
COMPONENT_GL:=false
endif
ifeq ($(HAVE_GL_H),)
ifeq ($(HAVE_GL_GL_H),)
COMPONENT_GL:=false
endif
endif
ifeq ($(HAVE_GLEW_LIB),)
COMPONENT_GL:=false
endif
ifeq ($(HAVE_GLEW_H),)
ifeq ($(HAVE_GL_GLEW_H),)
COMPONENT_GL:=false
endif
endif
COMPONENT_GL?=true

ifeq ($(HAVE_JACK_LIB),)
COMPONENT_JACK:=false
endif
ifeq ($(HAVE_JACK_H),)
COMPONENT_JACK:=false
endif
COMPONENT_JACK?=true

PYTHON?=python
COMPONENTS_SCRIPT:=$(MAKE_PATH)/components2code.py

$(COMPONENTS_HEADER): $(COMPONENTS_MAKE) $(COMPONENTS_SCRIPT) $(PROBE_MAKE)
	@echo "Creating $@..."
	@mkdir -p $(@D)
	@$(PYTHON) $(COMPONENTS_SCRIPT) \
		$(COMPONENT_CORE) $(COMPONENT_JACK) $(COMPONENT_GL) > $@
