# $Id$
#
# Create a zip file for Dingoo.

LOCAL_DIR:=$(BINDIST_DIR)/local
PACKAGE_SUPPORT_DIR:=build/package-dingux

# Override install locations.
INSTALL_BINARY_DIR:=$(LOCAL_DIR)/bin
INSTALL_SHARE_DIR:=$(LOCAL_DIR)/share/openmsx
INSTALL_DOC_DIR:=$(LOCAL_DIR)/doc/openmsx

BINDIST_OPENMSX_START:=$(INSTALL_BINARY_DIR)/openmsx-start.sh
BINDIST_README:=$(INSTALL_DOC_DIR)/README.txt

PACKAGE_FULL:=$(shell PYTHONPATH=build $(PYTHON) -c \
  "import version; print version.getVersionedPackageName()" \
  )
BINDIST_PACKAGE:=$(PACKAGE_FULL)-dingux-bin.zip

zip: install

bindist: zip $(BINDIST_OPENMSX_START) $(BINDIST_README)
	@echo "Creating zip file:"
	cd $(BINDIST_DIR) && zip ../$(BINDIST_PACKAGE) -r local

$(BINDIST_OPENMSX_START): $(PACKAGE_SUPPORT_DIR)/openmsx-start.sh install
$(BINDIST_README): $(PACKAGE_SUPPORT_DIR)/README.txt install
$(BINDIST_OPENMSX_START) $(BINDIST_README):
	@echo "  Copying $(@F)..."
	@mkdir -p $(@D)
	@cp $< $@
