# $Id$
#
# Compiles 3rd party libraries needed by openMSX.
# It enables only the features needed by openMSX: for example from SDL_image
# we only need PNG handling cability.

ifeq ($(origin PYTHON),undefined)
$(error You should pass PYTHON)
endif
ifeq ($(origin BUILD_PATH),undefined)
$(error You should pass BUILD_PATH)
endif
ifeq ($(origin OPENMSX_TARGET_OS),undefined)
$(error You should pass OPENMSX_TARGET_OS)
endif
ifeq ($(origin OPENMSX_TARGET_CPU),undefined)
$(error You should pass OPENMSX_TARGET_CPU)
endif

# Get information about the target OS.
SYSTEM_LIBS:=
include build/platform-$(OPENMSX_TARGET_OS).mk

# Compiler selection, compiler flags, SDK selection.
# These variables are already exported, but we make it explicit here.
export CC
export LD
export NEXT_ROOT
export MACOSX_DEPLOYMENT_TARGET

CC=$(_CC)
LD=$(_LD)

TARBALLS_DIR:=derived/3rdparty/download
SOURCE_DIR:=derived/3rdparty/src
PATCHES_DIR:=build/3rdparty
TIMESTAMP_DIR:=$(BUILD_PATH)/timestamps
BUILD_DIR:=$(BUILD_PATH)/build
INSTALL_DIR:=$(BUILD_PATH)/install

# Download locations for package sources.
DOWNLOAD_ZLIB:=http://downloads.sourceforge.net/libpng
DOWNLOAD_PNG:=http://downloads.sourceforge.net/libpng
DOWNLOAD_FREETYPE:=http://downloads.sourceforge.net/freetype
DOWNLOAD_SDL:=http://www.libsdl.org/release
DOWNLOAD_SDL_IMAGE:=http://www.libsdl.org/projects/SDL_image/release
DOWNLOAD_SDL_TTF:=http://www.libsdl.org/projects/SDL_ttf/release
DOWNLOAD_GLEW:=http://downloads.sourceforge.net/glew
DOWNLOAD_TCL:=http://downloads.sourceforge.net/tcl
DOWNLOAD_XML:=http://xmlsoft.org/sources
DOWNLOAD_DIRECTX:=http://alleg.sourceforge.net/files

# These were the most recent versions at the moment of writing this Makefile.
# You can use other versions if you like; adjust the names accordingly.
# Note: Do not put comments behind the definition, since this will include
#       a space in the value and spaces are separators for Make so the whole
#       build process will break.
PACKAGE_ZLIB:=zlib-1.2.3
PACKAGE_PNG:=libpng-1.2.34
# 2.3.8 is latest, but gives install problem?
PACKAGE_FREETYPE:=freetype-2.3.5
# 1.2.13 is latest, need to migrate patches
PACKAGE_SDL:=SDL-1.2.12
PACKAGE_SDL_IMAGE:=SDL_image-1.2.7
PACKAGE_SDL_TTF:=SDL_ttf-2.0.9
# 1.5.1 is latest, need to migrate patches
PACKAGE_GLEW:=glew-1.4.0
# 8.5.6 is latest, need to migrate patches
PACKAGE_TCL:=tcl8.4.19
# 2.7.3 is latest! Does it work?
PACKAGE_XML:=libxml2-2.6.32
PACKAGE_DIRECTX:=dx70

# Create a GNU-style system triple.
ifeq ($(OPENMSX_TARGET_CPU),x86)
TRIPLE_MACHINE:=i686
else
ifeq ($(OPENMSX_TARGET_CPU),ppc)
TRIPLE_MACHINE:=powerpc
else
ifeq ($(OPENMSX_TARGET_CPU),ppc64)
TRIPLE_MACHINE:=powerpc64
else
TRIPLE_MACHINE:=$(OPENMSX_TARGET_CPU)
endif
endif
endif
TRIPLE_OS:=$(OPENMSX_TARGET_OS)
TARGET_TRIPLE:=$(TRIPLE_MACHINE)-unknown-$(TRIPLE_OS)

# Although X11 is available on Windows and Mac OS X, most people do not have
# it installed, so do not link against it.
ifeq ($(filter linux freebsd netbsd openbsd gnu,$(OPENMSX_TARGET_OS)),)
USE_VIDEO_X11:=disable
else
USE_VIDEO_X11:=enable
endif

# Unfortunately not all packages stick to naming conventions such as putting
# the sources in a dir that includes the version number.
PACKAGES_STD:=ZLIB PNG FREETYPE SDL SDL_IMAGE SDL_TTF XML
PACKAGES_NONSTD:=GLEW TCL
PACKAGES_NOBUILD:=
ifeq ($(OPENMSX_TARGET_OS),mingw32)
PACKAGES_NOBUILD+=DIRECTX
endif
PACKAGES:=$(filter-out $(SYSTEM_LIBS),$(PACKAGES_STD) $(PACKAGES_NONSTD) $(PACKAGES_NOBUILD))
PACKAGES_BUILD:=$(filter-out $(PACKAGES_NOBUILD),$(PACKAGES))

# Source tar file names for non-standard packages.
TARBALL_GLEW:=$(PACKAGE_GLEW)-src.tgz
TARBALL_TCL:=$(PACKAGE_TCL)-src.tar.gz
TARBALL_DIRECTX:=$(PACKAGE_DIRECTX)_mgw.tar.gz
# Source tar file names for standard packages.
TARBALL_ZLIB:=$(PACKAGE_ZLIB).tar.gz
TARBALL_PNG:=$(PACKAGE_PNG).tar.gz
TARBALL_FREETYPE:=$(PACKAGE_FREETYPE).tar.gz
TARBALL_SDL:=$(PACKAGE_SDL).tar.gz
TARBALL_SDL_IMAGE:=$(PACKAGE_SDL_IMAGE).tar.gz
TARBALL_SDL_TTF:=$(PACKAGE_SDL_TTF).tar.gz
TARBALL_XML:=$(PACKAGE_XML).tar.gz

BUILD_TARGETS:=$(foreach PACKAGE,$(PACKAGES_BUILD),$(TIMESTAMP_DIR)/build-$(PACKAGE_$(PACKAGE)))
INSTALL_BUILD_TARGETS:=$(foreach PACKAGE,$(PACKAGES_BUILD),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))
INSTALL_NOBUILD_TARGETS:=$(foreach PACKAGE,$(PACKAGES_NOBUILD),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))

ifeq ($(filter $(PACKAGES),DIRECTX),)
INSTALL_DIRECTX:=
else
INSTALL_DIRECTX:=$(TIMESTAMP_DIR)/install-$(PACKAGE_DIRECTX)
endif

INSTALL_PARAMS_GLEW:=GLEW_DEST=$(PWD)/$(INSTALL_DIR)

# Function which, given a variable name prefix and the variable's value,
# returns the name of the package.
findpackage=$(strip $(foreach PACKAGE,$(PACKAGES),$(if $(filter $(2),$($(1)_$(PACKAGE))),$(PACKAGE),)))

.PHONY: all clean download

all: $(INSTALL_BUILD_TARGETS) $(INSTALL_NOBUILD_TARGETS)

clean:
	rm -rf $(SOURCE_DIR)
	rm -rf $(BUILD_DIR)
	rm -rf $(INSTALL_DIR)

# Install.
$(INSTALL_BUILD_TARGETS): $(TIMESTAMP_DIR)/install-%: $(TIMESTAMP_DIR)/build-%
	make -C $(BUILD_DIR)/$* install $(INSTALL_PARAMS_$(call findpackage,PACKAGE,$*))
	mkdir -p $(@D)
	touch $@

ifneq ($(INSTALL_DIRECTX),)
# Install DirectX headers.
$(INSTALL_DIRECTX): $(TARBALLS_DIR)/$(TARBALL_DIRECTX)
	mkdir -p $(INSTALL_DIR)
	tar -zxf $< -C $(INSTALL_DIR)
	mkdir -p $(@D)
	touch $@
endif

# Build.
$(BUILD_TARGETS): $(TIMESTAMP_DIR)/build-%: $(BUILD_DIR)/%/Makefile
	make -C $(<D) $(MAKEVAR_OVERRIDE_$(call findpackage,PACKAGE,$*))
	mkdir -p $(@D)
	touch $@

# Configuration of a lib can depend on the lib-config script of another lib.
# For example SDL_image depends on SDL and libpng.
PNG_CONFIG_SCRIPT:=$(INSTALL_DIR)/bin/libpng12-config
FREETYPE_CONFIG_SCRIPT:=$(INSTALL_DIR)/bin/freetype-config
SDL_CONFIG_SCRIPT:=$(INSTALL_DIR)/bin/sdl-config
$(PNG_CONFIG_SCRIPT): $(TIMESTAMP_DIR)/install-$(PACKAGE_PNG)
$(FREETYPE_CONFIG_SCRIPT): $(TIMESTAMP_DIR)/install-$(PACKAGE_FREETYPE)
$(SDL_CONFIG_SCRIPT): $(TIMESTAMP_DIR)/install-$(PACKAGE_SDL)

# Configure SDL.
$(BUILD_DIR)/$(PACKAGE_SDL)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL) $(INSTALL_DIRECTX)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--$(USE_VIDEO_X11)-video-x11 \
		--disable-video-directfb \
		--disable-directx \
		--disable-debug \
		--disable-cdrom \
		--disable-stdio-redirect \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) -L$(PWD)/$(INSTALL_DIR)/lib"
# While openMSX does not use "cpuinfo", "endian" and "file" modules, other
# modules do and if we disable them, SDL will not link.

# Configure SDL_image.
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
		--disable-png-shared \
		--disable-pnm \
		--disable-tga \
		--disable-tif \
		--disable-xcf \
		--disable-xpm \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS) $(shell $(PWD)/$(INSTALL_DIR)/bin/libpng12-config --cflags)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) $(shell $(PWD)/$(INSTALL_DIR)/bin/libpng12-config --static --ldflags)"

# Configure SDL_ttf.
$(BUILD_DIR)/$(PACKAGE_SDL_TTF)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL_TTF) \
  $(FREETYPE_CONFIG_SCRIPT) $(SDL_CONFIG_SCRIPT)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--disable-sdltest \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--with-sdl-prefix=$(PWD)/$(INSTALL_DIR) \
		--with-freetype-prefix=$(PWD)/$(INSTALL_DIR) \
		--$(subst disable,without,$(subst enable,with,$(USE_VIDEO_X11)))-x \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS)"

# Configure libpng.
$(BUILD_DIR)/$(PACKAGE_PNG)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_PNG) \
  $(foreach PACKAGE,$(filter-out $(SYSTEM_LIBS),ZLIB),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) -L$(PWD)/$(INSTALL_DIR)/lib"

# Configure FreeType.
$(BUILD_DIR)/$(PACKAGE_FREETYPE)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_FREETYPE)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) -L$(PWD)/$(INSTALL_DIR)/lib"

# Configure zlib.
# Although it uses "configure", zlib does not support building outside of the
# source tree, so just copy everything over (it's a small package).
$(BUILD_DIR)/$(PACKAGE_ZLIB)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_ZLIB)
	mkdir -p $(dir $(@D))
	rm -rf $(@D)
	cp -r $< $(@D)
	cd $(@D) && ./configure \
		--prefix=$(PWD)/$(INSTALL_DIR)
# It is not possible to pass CFLAGS to zlib's configure.
MAKEVAR_OVERRIDE_ZLIB:=CFLAGS="$(_CFLAGS)"
# Note: zlib's Makefile uses LDFLAGS to link its examples, not the library
#       itself. If we mess with it, the build breaks.

# Don't configure GLEW.
# GLEW does not support building outside of the source tree, so just copy
# everything over (it's a small package).
$(BUILD_DIR)/$(PACKAGE_GLEW)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_GLEW)
	mkdir -p $(dir $(@D))
	rm -rf $(@D)
	cp -r $< $(@D)
# GLEW does not have a configure script to pass CFLAGS to.
MAKEVAR_OVERRIDE_GLEW:=CC="$(_CC) $(_CFLAGS)" LD="$(_CC) $(_LDFLAGS)"

# Configure Tcl.
# Note: Tcl seems to build either dynamic libs or static libs, which is why we
#       have to pass --disable-shared to configure.
ifeq ($(OPENMSX_TARGET_OS),mingw32)
TCL_OS:=win
else
ifeq ($(OPENMSX_TARGET_OS),darwin)
TCL_OS:=maxosx
else
TCL_OS:=unix
endif
endif
$(BUILD_DIR)/$(PACKAGE_TCL)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_TCL)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</$(TCL_OS)/configure \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--disable-shared \
		--disable-threads \
		--disable-load
# Tcl's configure ignores CFLAGS passed to it.
MAKEVAR_OVERRIDE_TCL:=CFLAGS_OPTIMIZE="$(_CFLAGS)"

# Configure libxml2.
$(BUILD_DIR)/$(PACKAGE_XML)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_XML) \
  $(foreach PACKAGE,$(filter-out $(SYSTEM_LIBS),ZLIB),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--with-minimum \
		--with-push \
		--with-sax1 \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		$(if $(filter-out $(SYSTEM_LIBS),ZLIB),--with-zlib=$(PWD)/$(INSTALL_DIR),) \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) -L$(PWD)/$(INSTALL_DIR)/lib"

# Extract packages.
# Name mapping for standardized packages:
$(foreach PACKAGE,$(PACKAGES_STD),$(SOURCE_DIR)/$(PACKAGE_$(PACKAGE))): \
  $(SOURCE_DIR)/%: $(TARBALLS_DIR)/%.tar.gz
# Name mapping for GLEW:
$(SOURCE_DIR)/$(PACKAGE_GLEW): $(TARBALLS_DIR)/$(TARBALL_GLEW)
EXTRACTED_NAME_GLEW:=glew
# Name mapping for Tcl:
$(SOURCE_DIR)/$(PACKAGE_TCL): $(TARBALLS_DIR)/$(TARBALL_TCL)
# Extraction rule:
$(foreach PACKAGE,$(PACKAGES_BUILD),$(SOURCE_DIR)/$(PACKAGE_$(PACKAGE))):
	rm -rf $@
	mkdir -p $(@D)
	tar -zxf $< -C $(@D)
	if [ -n "$(EXTRACTED_NAME_$(call findpackage,TARBALL,$(<F)))" ]; then mv $(@D)/$(EXTRACTED_NAME_$(call findpackage,TARBALL,$(<F))) $@ ; fi
	test ! -e $(PATCHES_DIR)/$(PACKAGE_$(call findpackage,TARBALL,$(<F))).diff || patch -p1 -N -u -d $@ < $(PATCHES_DIR)/$(PACKAGE_$(call findpackage,TARBALL,$(<F))).diff
	touch $@

# Download source packages.
TARBALLS:=$(foreach PACKAGE,$(PACKAGES),$(TARBALLS_DIR)/$(TARBALL_$(PACKAGE)))
download: $(TARBALLS)
$(TARBALLS):
	mkdir -p $(@D)
	$(PYTHON) build/download.py \
		$(DOWNLOAD_$(call findpackage,TARBALL,$(@F)))/$(@F) $(@D)
