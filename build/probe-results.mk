# $Id$
#
# Display probe results, so user can decide whether to start the build,
# or to change system configuration and rerun "configure".

# This Makefile needs parameters to operate; check that they were specified:
# - PROBE_MAKE: Makefile with probe results
ifeq ($(PROBE_MAKE),)
$(error Missing parameter: PROBE_MAKE)
endif
# - MAKE_PATH: Directory containing global Makefiles
ifeq ($(MAKE_PATH),)
$(error Missing parameter: MAKE_PATH)
endif

include $(PROBE_MAKE)
include $(MAKE_PATH)/components.mk
include $(MAKE_PATH)/custom.mk

# Usage: $(call FOUND,LIB_NAME)
FOUND=$(if $(HAVE_$(1)_LIB),$(HAVE_$(1)_LIB),no)

# Usage: $(call YESNO,true|false)
YESNO=$(subst true,yes,$(subst false,no,$(1)))

# Usage: $(call HEADER,LIB1 LIB2 ...)
HEADER=$(if $(strip $(foreach LIB,$(1),$(HAVE_$(LIB)_H))),yes,no)

all:
	@echo ""
	@echo "Found libraries:"
	@echo "  libpng:          $(call FOUND,PNG)"
	@echo "  libxml2:         $(call FOUND,XML)"
	@echo "  OpenGL:          $(call FOUND,GL)"
	@echo "  SDL:             $(call FOUND,SDL)"
	@echo "  SDL_image:       $(call FOUND,SDL_IMAGE)"
	@echo "  TCL:             $(call FOUND,TCL)"
	@echo "  zlib:            $(call FOUND,ZLIB)"
	@echo ""
	@echo "Found headers:"
	@echo "  libpng:          $(call HEADER,PNG)"
	@echo "  libxml2:         $(call HEADER,XML)"
	@echo "  OpenGL:          $(call HEADER,GL GL_GL)"
	@echo "  SDL:             $(call HEADER,SDL)"
	@echo "  SDL_image:       $(call HEADER,SDL_IMAGE)"
	@echo "  TCL:             $(call HEADER,TCL)"
	@echo "  zlib:            $(call HEADER,ZLIB)"
	@echo ""
	@echo "Components overview:"
	@echo "  Emulation core:  $(call YESNO,$(COMPONENT_CORE))"
	@echo "  SDLGL renderer:  $(call YESNO,$(COMPONENT_GL))"
	@echo ""
	@echo "Customisable options:"
	@echo "  Install to:      $(INSTALL_BASE)"
	@echo "  (you can edit these in build/custom.mk)"
	@echo ""
ifeq ($(COMPONENTS_ALL),true)
	@echo "All required and optional components can be built."
else
ifeq ($(COMPONENT_CORE),true)
	@echo "If you are satisfied with the probe results, run \"$(MAKE)\" to start the build."
	@echo "Otherwise, install some libraries and headers and rerun \"configure\"."
else
	@echo "Please install missing libraries and headers and rerun \"configure\"."
endif
endif
	@echo ""

