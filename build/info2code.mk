# $Id$
#
# Write build info to C++ constants, so source can access it.
# Advantages of this approach:
# - file dates used for dependency checks (as opposed to "-D" compile flag)
# - inactive code is still checked by compiler (as opposed to "#if")

PYTHON?=python
BUILDINFO_SCRIPT:=$(MAKE_PATH)/buildinfo2code.py
VERSION_SCRIPT:=$(MAKE_PATH)/version2code.py

$(CONFIG_HEADER): $(BUILDINFO_SCRIPT) $(MAKE_PATH)/custom.mk
	@echo "Creating $@..."
	@mkdir -p $(@D)
	@$(PYTHON) $(BUILDINFO_SCRIPT) \
		$(OPENMSX_TARGET_OS) $(OPENMSX_TARGET_CPU) $(OPENMSX_FLAVOUR) \
		$(INSTALL_SHARE_DIR) > $@

$(VERSION_HEADER): $(VERSION_SCRIPT) ChangeLog $(MAKE_PATH)/version.mk
	@echo "Creating $@..."
	@mkdir -p $(@D)
	@$(PYTHON) $(VERSION_SCRIPT) > $@
