# $Id$
#
# Create binary distribution directory.

BINDIST_DIR:=$(BUILD_PATH)/bindist
BINDIST_PACKAGE:=

# Override install locations.
# These can be overridden again in platform-specific bindist sections.
INSTALL_ROOT:=$(BINDIST_DIR)/install
INSTALL_BINARY_DIR:=$(INSTALL_ROOT)/bin
INSTALL_SHARE_DIR:=$(INSTALL_ROOT)/share
INSTALL_DOC_DIR:=$(INSTALL_ROOT)/doc
# C-BIOS should be included.
INSTALL_CONTRIB:=true
# Do not display header and post-install instructions.
INSTALL_VERBOSE:=false

.PHONY: bindist bindistclean

bindist: install

# Force removal of old destination dir before installing to new dir.
install: bindistclean

bindistclean: $(BINARY_FULL)
	@echo "Removing any old binary package..."
	@rm -rf $(BINDIST_DIR)
	@$(if $(BINDIST_PACKAGE),rm -f $(BINDIST_PACKAGE),)
	@echo "Creating binary package:"
