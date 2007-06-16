# $Id$
#
# Compiles 3rd party libraries needed by openMSX.
# Actually, only those 3rd party libraries that are not in the default system
# of Mac OS X, but others can be added later.
# It enables only the features needed by openMSX: for example from SDL_image
# we only need PNG handling cability.

ifeq ($(origin BUILD_PATH),undefined)
$(error You should pass BUILD_PATH)
endif

# Compiler selection, compiler flags, SDK selection.
# These variables are already exported, but we make it explicit here.
export CC
export LD
export NEXT_ROOT

TARBALLS_DIR:=derived/3rdparty/download
SOURCE_DIR:=derived/3rdparty/src
PATCHES_DIR:=build/3rdparty
TIMESTAMP_DIR:=$(BUILD_PATH)/timestamps
BUILD_DIR:=$(BUILD_PATH)/build
INSTALL_DIR:=$(BUILD_PATH)/install

# Download locations for package sources.
DOWNLOAD_PNG:=http://downloads.sourceforge.net/libpng
DOWNLOAD_SDL:=http://www.libsdl.org/release
DOWNLOAD_SDL_IMAGE:=http://www.libsdl.org/projects/SDL_image/release
DOWNLOAD_GLEW:=http://downloads.sourceforge.net/glew

# These were the most recent versions at the moment of writing this Makefile.
# You can use other versions if you like; adjust the names accordingly.
PACKAGE_PNG:=libpng-1.2.18
PACKAGE_SDL:=SDL-1.2.11
PACKAGE_SDL_IMAGE:=SDL_image-1.2.5
PACKAGE_GLEW:=glew-1.4.0

# Unfortunately not all packages stick to naming conventions such as putting
# the sources in a dir that includes the version number.
PACKAGES_STD:=PNG SDL SDL_IMAGE
PACKAGES_NONSTD:=GLEW
PACKAGES:=$(PACKAGES_STD) $(PACKAGES_NONSTD)

# Source tar file names for non-standard packages.
TARBALL_GLEW:=$(PACKAGE_GLEW)-src.tgz
# Source tar file names for standard packages.
TARBALL_PNG:=$(PACKAGE_PNG).tar.gz
TARBALL_SDL:=$(PACKAGE_SDL).tar.gz
TARBALL_SDL_IMAGE:=$(PACKAGE_SDL_IMAGE).tar.gz
# All source tar file names.
TARBALLS:=$(foreach PACKAGE,$(PACKAGES),$(TARBALL_$(PACKAGE)))

BUILD_TARGETS:=$(foreach PACKAGE,$(PACKAGES),$(TIMESTAMP_DIR)/build-$(PACKAGE_$(PACKAGE)))
INSTALL_TARGETS:=$(foreach PACKAGE,$(PACKAGES),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))

INSTALL_PARAMS_GLEW:=GLEW_DEST=$(PWD)/$(INSTALL_DIR)

CURL_TEST:=$(TIMESTAMP_DIR)/check-curl

# Function which, given a variable name prefix and the variable's value,
# returns the name of the package.
findpackage=$(strip $(foreach PACKAGE,$(PACKAGES),$(if $(filter $(2),$($(1)_$(PACKAGE))),$(PACKAGE),)))

.PHONY: all clean download

all: $(INSTALL_TARGETS)

clean:
	rm -rf $(SOURCE_DIR)
	rm -rf $(BUILD_DIR)
	rm -rf $(INSTALL_DIR)

# Install.
$(INSTALL_TARGETS): $(TIMESTAMP_DIR)/install-%: $(TIMESTAMP_DIR)/build-%
	make -C $(BUILD_DIR)/$* install $(INSTALL_PARAMS_$(call findpackage,PACKAGE,$*))
	mkdir -p $(@D)
	touch $@

# Build.
$(BUILD_TARGETS): $(TIMESTAMP_DIR)/build-%: $(BUILD_DIR)/%/Makefile
	make -C $(<D)
	mkdir -p $(@D)
	touch $@

# Configure SDL.
$(BUILD_DIR)/$(PACKAGE_SDL)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--disable-video-x11 \
		--disable-debug \
		--disable-cdrom \
		--disable-stdio-redirect \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)"
# While openMSX does not use "cpuinfo", "endian" and "file" modules, other
# modules do and if we disable them, SDL will not link.

# Configure SDL_image.
PNG_CONFIG_SCRIPT:=$(INSTALL_DIR)/bin/libpng12-config
SDL_CONFIG_SCRIPT:=$(INSTALL_DIR)/bin/sdl-config
$(PNG_CONFIG_SCRIPT): $(TIMESTAMP_DIR)/install-$(PACKAGE_PNG)
$(SDL_CONFIG_SCRIPT): $(TIMESTAMP_DIR)/install-$(PACKAGE_SDL)
$(BUILD_DIR)/$(PACKAGE_SDL_IMAGE)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL_IMAGE) $(PNG_CONFIG_SCRIPT) $(SDL_CONFIG_SCRIPT)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--disable-sdltest \
		--disable-bmp \
		--disable-gif \
		--disable-jpg \
		--disable-lbm \
		--disable-pcx \
		--enable-png \
		--disable-pnm \
		--disable-tga \
		--disable-tif \
		--disable-xcf \
		--disable-xpm \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS) $(shell $(PWD)/$(INSTALL_DIR)/bin/libpng12-config --cflags)"

# Configure libpng.
$(BUILD_DIR)/$(PACKAGE_PNG)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_PNG)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)"

# Don't configure GLEW.
# GLEW does not support building outside of the source tree, so just copy
# everything over (it's a small package).
$(BUILD_DIR)/$(PACKAGE_GLEW)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_GLEW)
	mkdir -p $(dir $(@D))
	rm -rf $(@D)
	cp -r $< $(@D)

# Extract packages.
$(foreach PACKAGE,$(PACKAGES_STD),$(SOURCE_DIR)/$(PACKAGE_$(PACKAGE))): \
  $(SOURCE_DIR)/%: $(TARBALLS_DIR)/%.tar.gz
	mkdir -p $(@D)
	tar -zxf $< -C $(@D)
	test ! -e $(PATCHES_DIR)/$(<F:%.tar.gz=%.diff) || patch -p1 -N -u -d $@ < $(PATCHES_DIR)/$(<F:%.tar.gz=%.diff)
	touch $@

# Extract GLEW.
$(SOURCE_DIR)/$(PACKAGE_GLEW): $(TARBALLS_DIR)/$(TARBALL_GLEW)
	rm -rf $@
	mkdir -p $(@D)
	tar -zxf $< -C $(@D)
	mv $(@D)/glew $@
	test ! -e $(PATCHES_DIR)/$(<F:%-src.tgz=%.diff) || patch -p1 -N -u -d $@ < $(PATCHES_DIR)/$(<F:%-src.tgz=%.diff)
	touch $@

# Check whether CURL is available.
$(CURL_TEST):
	curl --version ; if [ $$? != 2 ]; then echo "Please install CURL (http://curl.haxx.se/) and put it in the PATH."; false; fi
	mkdir -p $(@D)
	touch $@

# Download source packages.
TARBALLS:=$(foreach PACKAGE,$(PACKAGES),$(TARBALLS_DIR)/$(TARBALL_$(PACKAGE)))
download: $(TARBALLS)
$(TARBALLS): $(CURL_TEST)
	mkdir -p $(@D)
	curl --fail --location -o $@ $(DOWNLOAD_$(call findpackage,TARBALL,$(@F)))/$(@F)
