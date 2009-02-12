# $Id$
#
# Display probe results, so user can decide whether to start the build,
# or to change system configuration and rerun "configure".

# This Makefile needs parameters to operate; check that they were specified:
# - PROBE_MAKE: Makefile with probe results
ifeq ($(PROBE_MAKE),)
$(error Missing parameter: PROBE_MAKE)
endif

include $(PROBE_MAKE)

PYTHON?=python

.PHONY: all

all:
	@$(PYTHON) build/probe-results.py $(PROBE_MAKE)
