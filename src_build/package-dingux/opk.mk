# Create an OPK file for OpenDingux.

PACKAGE_SUPPORT_DIR:=src_build/package-dingux

# Override install locations.
DESTDIR:=$(BINDIST_DIR)
INSTALL_BINARY_DIR:=bin
INSTALL_SHARE_DIR:=share
INSTALL_DOC_DIR:=doc

BINDIST_OPENMSX_START:=$(DESTDIR)/$(INSTALL_BINARY_DIR)/openmsx-start.sh

PACKAGE_FULL:=$(shell PYTHONPATH=build $(PYTHON) -c \
  "import version; print(version.getVersionedPackageName())" \
  )
BINDIST_PACKAGE:=$(PACKAGE_FULL).opk

bindist: install $(BINDIST_OPENMSX_START)
	@echo "Creating OPK file:"
	mksquashfs \
		$(PACKAGE_SUPPORT_DIR)/default.gcw0.desktop \
		share/icons/openMSX-logo-32.png \
		$(PACKAGE_SUPPORT_DIR)/manual.man.txt \
		$(BINDIST_DIR)/* \
		$(BINDIST_DIR)/../$(BINDIST_PACKAGE) \
		-noappend -comp gzip -all-root

$(BINDIST_OPENMSX_START): $(PACKAGE_SUPPORT_DIR)/openmsx-start.sh install
	@echo "  Copying $(@F)..."
	@mkdir -p $(@D)
	@cp $< $@
