# $Id$
# Defines the building blocks of openMSX and their dependencies.

PYTHON?=python
COMPONENTS_HEADER_SCRIPT:=$(MAKE_PATH)/components2code.py
COMPONENTS_DEFS_SCRIPT:=$(MAKE_PATH)/components2defs.py

$(COMPONENTS_HEADER): $(COMPONENTS_MAKE) $(COMPONENTS_HEADER_SCRIPT) $(PROBE_MAKE)
	@echo "Creating $@..."
	@mkdir -p $(@D)
	@$(PYTHON) $(COMPONENTS_HEADER_SCRIPT) $(PROBE_MAKE) > $@

$(COMPONENTS_DEFS): $(COMPONENTS_MAKE) $(COMPONENTS_DEFS_SCRIPT) $(PROBE_MAKE)
	@echo "Creating $@..."
	@mkdir -p $(@D)
	@$(PYTHON) $(COMPONENTS_DEFS_SCRIPT) $(PROBE_MAKE) > $@
