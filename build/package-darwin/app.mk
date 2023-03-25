# Create an application directory for Darwin.

APP_SUPPORT_DIR:=build/package-darwin
APP_DIR:=openMSX.app
APP_EXE_DIR:=$(APP_DIR)/Contents/MacOS
APP_PLIST:=$(APP_DIR)/Contents/Info.plist
APP_RES:=$(APP_DIR)/Contents/Resources
APP_ICON:=$(APP_RES)/openmsx-logo.icns

# Override install locations.
DESTDIR:=$(BINDIST_DIR)
INSTALL_BINARY_DIR:=$(APP_EXE_DIR)
INSTALL_SHARE_DIR:=$(APP_RES)/share
INSTALL_DOC_DIR:=Documentation

PACKAGE_FULL:=$(shell PYTHONPATH=build $(PYTHON) -c \
  "import version; print(version.getVersionedPackageName())" \
  )
BINDIST_PACKAGE:=$(BUILD_PATH)/$(PACKAGE_FULL)-mac-$(OPENMSX_TARGET_CPU)-bin.dmg
BINDIST_README:=README.html
BINDIST_LICENSE:=$(INSTALL_DOC_DIR)/GPL.txt

# TODO: What is needed for an app folder?
app: install $(DESTDIR)/$(APP_PLIST) $(DESTDIR)/$(APP_ICON)

bindist: app codesign $(DESTDIR)/$(BINDIST_README) $(DESTDIR)/$(BINDIST_LICENSE)
	@echo "Creating disk image:"
	@hdiutil create -srcfolder $(BINDIST_DIR) \
		-fs HFS+J \
		-volname openMSX \
		-imagekey zlib-level=9 \
		-ov $(BINDIST_PACKAGE)

$(DESTDIR)/$(APP_PLIST): $(DESTDIR)/$(APP_DIR)/Contents/%: $(APP_SUPPORT_DIR)/% bindistclean
	@echo "  Writing meta-info..."
	@mkdir -p $(@D)
	@sed -e 's/%ICON%/$(notdir $(APP_ICON))/' \
		-e 's/%VERSION%/$(shell $(PYTHON) build/version.py triple)/' < $< > $@
	@echo "APPLoMSX" > $(@D)/PkgInfo

$(DESTDIR)/$(APP_ICON): $(DESTDIR)/$(APP_RES)/%: $(APP_SUPPORT_DIR)/% bindistclean
	@echo "  Copying resources..."
	@mkdir -p $(@D)
	@cp $< $@

codesign: app
	@if [ -z "$(CODE_SIGN_IDENTITY)" ]; then \
		echo "  Skipping code sign, CODE_SIGN_IDENTITY not set."; \
	else \
		echo "  Signing the application bundle..."; \
		codesign --deep --force --verify --verbose --options runtime --sign "$(CODE_SIGN_IDENTITY)" $(DESTDIR)/$(APP_DIR); \
	fi

$(DESTDIR)/$(BINDIST_README): $(APP_SUPPORT_DIR)/README.html
	@echo "  Copying README..."
	@mkdir -p $(@D)
	@cp $< $@

$(DESTDIR)/$(BINDIST_LICENSE): doc/GPL.txt app
	@echo "  Copying license..."
	@mkdir -p $(@D)
# Remove form feeds from the GPL document, so Safari will treat it as text.
	@awk '!/\f/ ; /\f/ { print "" }' $< > $@
