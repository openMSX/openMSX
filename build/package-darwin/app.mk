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
# uses the plain release version, not the git revision. Longer than 27
# characters is fine: the background alias in .DS_Store truncates the name in
# its classic 27-byte field, but also stores the full name, which Finder uses.
DMG_VOLNAME:=openMSX $(shell PYTHONPATH=build $(PYTHON) -c \
  "import version; print(version.packageVersion)" \
  ) installer

# TODO: What is needed for an app folder?
app: install $(DESTDIR)/$(APP_PLIST) $(DESTDIR)/$(APP_ICON)

bindist: app codesign $(DESTDIR)/$(BINDIST_LICENSE)
	@echo "Creating disk image:"
# dmgbuild (pip install dmgbuild) writes the Finder layout for the background
# and icon positions. Without it, fall back to a plain, unstyled disk image,
# unless REQUIRE_DMGBUILD is set (as CI does), so a release can't silently lose
# its layout.
	@if command -v dmgbuild > /dev/null; then \
		dmgbuild -s $(APP_SUPPORT_DIR)/dmg_settings.py \
			-D app=$(DESTDIR)/$(APP_DIR) \
			-D background=$(APP_SUPPORT_DIR)/dmg_bg.tiff \
			"$(DMG_VOLNAME)" $(BINDIST_PACKAGE); \
	elif [ -n "$(REQUIRE_DMGBUILD)" ]; then \
		echo "  dmgbuild not found, but REQUIRE_DMGBUILD is set." >&2; \
		exit 1; \
	else \
		echo "  dmgbuild not found, creating an unstyled disk image."; \
		ln -sf /Applications $(BINDIST_DIR)/Applications; \
		hdiutil create -srcfolder $(BINDIST_DIR) \
			-fs HFS+J \
			-volname "$(DMG_VOLNAME)" \
			-imagekey zlib-level=9 \
			-ov $(BINDIST_PACKAGE); \
	fi
	@if [ -z "$(CODE_SIGN_IDENTITY)" ]; then \
		echo "  Skipping code sign, CODE_SIGN_IDENTITY not set."; \
	else \
		echo "  Signing the disk image..."; \
		codesign --force --verify --verbose --sign "$(CODE_SIGN_IDENTITY)" "$(BINDIST_PACKAGE)"; \
	fi
# Stapling and validating contact Apple's servers, which sometimes lose the
# connection, so retry those a few times.
	@if [ -z "$(NOTARY_PROFILE)" ]; then \
		echo "  Skipping notarization, NOTARY_PROFILE not set."; \
	else \
		echo "  Notarizing the disk image..."; \
		xcrun notarytool submit "$(BINDIST_PACKAGE)" --keychain-profile "$(NOTARY_PROFILE)" --wait || exit 1; \
		for action in staple validate; do \
			for attempt in 1 2 3 4 5; do \
				xcrun stapler $$action "$(BINDIST_PACKAGE)" && break; \
				if [ $$attempt = 5 ]; then exit 1; fi; \
				echo "  Stapler $$action failed, retrying in 30 seconds..."; \
				sleep 30; \
			done; \
		done; \
	fi

$(DESTDIR)/$(APP_PLIST): $(DESTDIR)/$(APP_DIR)/Contents/%: $(APP_SUPPORT_DIR)/% bindistclean
	@echo "  Writing meta-info..."
	@mkdir -p $(@D)
	@sed -e 's/%ICON%/$(notdir $(APP_ICON))/' \
		-e 's/%VERSION%/$(shell $(PYTHON) build/version.py triple)/' \
		-e 's/%MIN_OS_VERSION%/$(OSX_VER)/' < $< > $@
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
