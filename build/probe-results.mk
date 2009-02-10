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

PYTHON?=python

.PHONY: all

all:
	@$(PYTHON) build/probe-results.py \
		$(PROBE_MAKE) $(COMPONENT_CORE) $(COMPONENT_JACK) $(COMPONENT_GL)
