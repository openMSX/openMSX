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
INSTALL_DOC_DIR:=$(APP_RES)/doc

PACKAGE_FULL:=$(shell PYTHONPATH=build $(PYTHON) -c \
  "import version; print(version.getVersionedPackageName())" \
  )
BINDIST_PACKAGE:=$(BUILD_PATH)/$(PACKAGE_FULL)-mac-$(OPENMSX_TARGET_CPU)-bin.dmg
BINDIST_LICENSE:=$(INSTALL_DOC_DIR)/GPL.txt

# Shown as the DMG window title and in the Finder path/status bar. Deliberately
# uses the plain release version, not the git revision: the committed DS_Store
# embeds this volume name, so it has to be stable between builds. Keep it under
# 27 characters, and regenerate DS_Store with create-dmg.sh if it ever changes,
# passing this same name as the volume name.
DMG_VOLNAME:=openMSX $(shell PYTHONPATH=build $(PYTHON) -c \
  "import version; print(version.packageVersion)" \
  ) installer

# TODO: What is needed for an app folder?
app: install $(DESTDIR)/$(APP_PLIST) $(DESTDIR)/$(APP_ICON)

bindist: app codesign $(DESTDIR)/$(BINDIST_LICENSE)
	@echo "Creating disk image:"
	@cp $(APP_SUPPORT_DIR)/DS_Store $(BINDIST_DIR)/.DS_Store || true
	@mkdir -p $(BINDIST_DIR)/.background
# Combine the 1x and 2x backgrounds into one multi-resolution TIFF so the image
# stays sharp on Retina. Must match what create-dmg.sh generates, since the
# committed DS_Store references .background/dmg_bg.tiff by name.
	@tiffutil -cathidpicheck $(APP_SUPPORT_DIR)/dmg_bg.png \
		$(APP_SUPPORT_DIR)/dmg_bg@2x.png \
		-out $(BINDIST_DIR)/.background/dmg_bg.tiff > /dev/null
	@ln -sf /Applications $(BINDIST_DIR)/Applications
	@hdiutil create -srcfolder $(BINDIST_DIR) \
		-fs HFS+J \
		-volname "$(DMG_VOLNAME)" \
		-imagekey zlib-level=9 \
		-ov $(BINDIST_PACKAGE)
	@if [ -z "$(CODE_SIGN_IDENTITY)" ]; then \
		echo "  Skipping code sign, CODE_SIGN_IDENTITY not set."; \
	else \
		echo "  Signing the disk image..."; \
		codesign --force --verify --verbose --sign "$(CODE_SIGN_IDENTITY)" "$(BINDIST_PACKAGE)"; \
	fi
	@if [ -z "$(NOTARY_PROFILE)" ]; then \
		echo "  Skipping notarization, NOTARY_PROFILE not set."; \
	else \
		echo "  Notarizing the disk image..."; \
		xcrun notarytool submit "$(BINDIST_PACKAGE)" --keychain-profile "$(NOTARY_PROFILE)" --wait; \
		xcrun stapler staple "$(BINDIST_PACKAGE)"; \
		xcrun stapler validate "$(BINDIST_PACKAGE)"; \
	fi

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

$(DESTDIR)/$(BINDIST_LICENSE): doc/GPL.txt app
	@echo "  Copying license..."
	@mkdir -p $(@D)
# Remove form feeds from the GPL document, so Safari will treat it as text.
	@awk '!/\f/ ; /\f/ { print "" }' $< > $@
