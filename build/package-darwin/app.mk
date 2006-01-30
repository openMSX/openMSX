# $Id$
#
# Create an application directory for Darwin.

BINDIST_DIR:=$(BUILD_PATH)/bindist
BINDIST_IMAGE:=$(BUILD_PATH)/$(PACKAGE_FULL)-$(OPENMSX_TARGET_CPU)-bin.dmg

APP_SUPPORT_DIR:=build/package-darwin
APP_DIR:=$(BINDIST_DIR)/openMSX.app
APP_EXE_DIR:=$(APP_DIR)/Contents/MacOS
APP_PLIST:=$(APP_DIR)/Contents/Info.plist
APP_RES:=$(APP_DIR)/Contents/Resources
APP_ICON:=$(APP_RES)/openmsx-logo.icns

# Override install locations.
INSTALL_BINARY_DIR:=$(APP_EXE_DIR)
INSTALL_SHARE_DIR:=$(APP_DIR)/share
INSTALL_DOC_DIR:=$(BINDIST_DIR)/Documentation
# C-BIOS should be included.
INSTALL_CONTRIB:=true
# Do not display header and post-install instructions.
INSTALL_VERBOSE:=false

.PHONY: bindist bindistclean

bindist: install $(APP_PLIST) $(APP_ICON)
	@echo "Creating disk image:"
	@hdiutil create -srcfolder $(BINDIST_DIR) \
		-volname openMSX \
		-imagekey zlib-level=9 \
		-ov $(BINDIST_IMAGE)
	@hdiutil internet-enable -yes $(BINDIST_IMAGE)

# Force removal of old app before installing to new app.
install: bindistclean

bindistclean: $(BINARY_FULL)
	@echo "Removing any old binary package..."
	@rm -rf $(BINDIST_DIR)
	@echo "Creating binary package:"

$(APP_PLIST): $(APP_DIR)/Contents/%: $(APP_SUPPORT_DIR)/% bindistclean
	@echo "  Writing meta-info..."
	@mkdir -p $(@D)
	@sed -e 's/%ICON%/$(notdir $(APP_ICON))/' \
		-e 's/%VERSION%/$(PACKAGE_VERSION)/' < $< > $@
	@echo "APPLoMSX" > $(@D)/PkgInfo

$(APP_ICON): $(APP_RES)/%: $(APP_SUPPORT_DIR)/% bindistclean
	@echo "  Copying resources..."
	@mkdir -p $(@D)
	@cp $< $@
