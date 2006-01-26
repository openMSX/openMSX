# $Id$
#
# Compiles 3rd party libraries needed by openMSX.
# Actually, only those 3rd party libraries that are not in the default system
# of Mac OS X, but others can be added later.
# It enables only the features needed by openMSX: for example from SDL_image
# we only need PNG handling cability.

SOURCES_DIR:=3rdparty
STATIC_DIR:=derived/3rdparty

# These were the most recent versions at the moment of writing this Makefile.
# You can use other versions if you like; adjust the names accordingly.
PACKAGE_PNG:=libpng-1.2.8-config
PACKAGE_SDL:=SDL-1.2.9
PACKAGE_SDL_IMAGE:=SDL_image-1.2.4

PACKAGES:=PNG SDL SDL_IMAGE

BUILD_TARGETS:=$(addprefix build-,$(PACKAGES))
INSTALL_TARGETS:=$(addprefix install-,$(PACKAGES))

# We want a small distribution set, so optimize for size.
# We got pretty good results with -Os in benchmarks, so it is not likely to
# harm performance.
export CFLAGS:=-Os

.PHONY: all $(BUILD_TARGETS) $(INSTALL_TARGETS)

all: $(INSTALL_TARGETS)

# Install.
$(INSTALL_TARGETS): install-%: build-%
	make -C $(STATIC_DIR)/$< install

# Build.
$(BUILD_TARGETS): build-%: $(STATIC_DIR)/build-%/Makefile
	make -C $(STATIC_DIR)/$@

# Configure SDL.
$(STATIC_DIR)/build-SDL/Makefile: \
  $(STATIC_DIR)/$(PACKAGE_SDL)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--disable-debug \
		--disable-cdrom \
		--prefix=$(PWD)/$(STATIC_DIR)/install
# While openMSX does not use "cpuinfo", "endian" and "file" modules, other
# modules do and if we disable them, SDL will not link.

# Configure SDL_image.
$(STATIC_DIR)/build-SDL_IMAGE/Makefile: \
  $(STATIC_DIR)/$(PACKAGE_SDL_IMAGE) install-SDL install-PNG
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
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
		--prefix=$(PWD)/$(STATIC_DIR)/install \
		CFLAGS=-I$(PWD)/$(STATIC_DIR)/install/include/libpng
# Note: libpng-config returns the "include" dir, while SDL_image expects
#       "include/libpng".

# Configure libpng.
$(STATIC_DIR)/build-PNG/Makefile: \
  $(STATIC_DIR)/$(PACKAGE_PNG)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--prefix=$(PWD)/$(STATIC_DIR)/install

# Extract.
$(foreach PACKAGE,$(PACKAGES),$(STATIC_DIR)/$(PACKAGE_$(PACKAGE))): \
  $(STATIC_DIR)/%: $(SOURCES_DIR)/%.tar.gz
	mkdir -p $(STATIC_DIR)
	tar -zxf $< -C $(STATIC_DIR)
	touch $@
