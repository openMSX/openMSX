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
DOWNLOAD_ZLIB:=http://downloads.sourceforge.net/libpng
DOWNLOAD_PNG:=http://downloads.sourceforge.net/libpng
DOWNLOAD_SDL:=http://www.libsdl.org/release
DOWNLOAD_SDL_IMAGE:=http://www.libsdl.org/projects/SDL_image/release
DOWNLOAD_GLEW:=http://downloads.sourceforge.net/glew
DOWNLOAD_TCL:=http://downloads.sourceforge.net/tcl
DOWNLOAD_XML:=http://xmlsoft.org/sources

# These were the most recent versions at the moment of writing this Makefile.
# You can use other versions if you like; adjust the names accordingly.
PACKAGE_ZLIB:=zlib-1.2.3
PACKAGE_PNG:=libpng-1.2.19
PACKAGE_SDL:=SDL-1.2.12
PACKAGE_SDL_IMAGE:=SDL_image-1.2.6
PACKAGE_GLEW:=glew-1.4.0
PACKAGE_TCL:=tcl8.4.15
PACKAGE_XML:=libxml2-2.6.29

# Check OS.
# This is done for Tcl, but we can also use the result ourselves.
# TODO: We want to use this Makefile for cross compilation in the future,
#       which means we should not probe the local system except to provide a
#       default OS when the user did not specify one.
TCL_OS_TEST:=case `uname -s` in MINGW*) echo "win";; Darwin) echo "macosx";; *) echo "unix";; esac
TCL_OS:=$(shell $(TCL_OS_TEST))

# Depending on the platform, some libraries are already available system-wide.
ifeq ($(TCL_OS),macosx)
SYSTEM_LIBS:=ZLIB TCL XML
else
# On Windows none of the required libraries are part of the base system.
# On Unix-like systems the libraries we use are often installed as dependencies
# of installed applications, but not part of the base system. Also, which
# optional dependencies are enabled for SDL and SDL_image varies. So if we want
# to produce a binary that has a reasonable chance of running on many machines,
# we cannot assume any library to be part of the base system.
SYSTEM_LIBS:=
endif

# Although X11 is available on Windows and Mac OS X, most people do not have
# it installed, so do not link against it.
ifeq ($(TCL_OS),unix)
USE_VIDEO_X11:=enable
else
USE_VIDEO_X11:=disable
endif

# Unfortunately not all packages stick to naming conventions such as putting
# the sources in a dir that includes the version number.
PACKAGES_STD:=ZLIB PNG SDL SDL_IMAGE XML
PACKAGES_NONSTD:=GLEW TCL
PACKAGES:=$(filter-out $(SYSTEM_LIBS),$(PACKAGES_STD) $(PACKAGES_NONSTD))

# Source tar file names for non-standard packages.
TARBALL_GLEW:=$(PACKAGE_GLEW)-src.tgz
TARBALL_TCL:=$(PACKAGE_TCL)-src.tar.gz
# Source tar file names for standard packages.
TARBALL_ZLIB:=$(PACKAGE_ZLIB).tar.gz
TARBALL_PNG:=$(PACKAGE_PNG).tar.gz
TARBALL_SDL:=$(PACKAGE_SDL).tar.gz
TARBALL_SDL_IMAGE:=$(PACKAGE_SDL_IMAGE).tar.gz
TARBALL_XML:=$(PACKAGE_XML).tar.gz

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
	make -C $(<D) $(MAKEVAR_OVERRIDE_$(call findpackage,PACKAGE,$*))
	mkdir -p $(@D)
	touch $@

# Configure SDL.
$(BUILD_DIR)/$(PACKAGE_SDL)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--$(USE_VIDEO_X11)-video-x11) \
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
		CFLAGS="$(_CFLAGS) $(shell $(PWD)/$(INSTALL_DIR)/bin/libpng12-config --cflags)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(shell $(PWD)/$(INSTALL_DIR)/bin/libpng12-config --static --ldflags)"

# Configure libpng.
# The "-I$(PWD)/$<" part of the CPPFLAGS definition is a workaround for a bug
# in the configure script of libpng 1.2.18 (reported as bug 1738534).
$(BUILD_DIR)/$(PACKAGE_PNG)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_PNG) \
  $(foreach PACKAGE,$(filter-out $(SYSTEM_LIBS),ZLIB),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include -I$(PWD)/$<" \
		LDFLAGS=-L$(PWD)/$(INSTALL_DIR)/lib

# Configure zlib.
# Although it uses "configure", zlib does not support building outside of the
# source tree, so just copy everything over (it's a small package).
# You can configure zlib to build either a static lib, or a dynamic one, not
# both. So we configure it for both types, store the resulting Makefiles and
# replace the main Makefile to invoke both actual Makefiles.
$(BUILD_DIR)/$(PACKAGE_ZLIB)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_ZLIB)
	mkdir -p $(dir $(@D))
	rm -rf $(@D)
	cp -r $< $(@D)
	cd $(@D) && ./configure \
		--prefix=$(PWD)/$(INSTALL_DIR)
	mv $(@D)/Makefile $(@D)/Makefile.static
	cd $(@D) && ./configure \
		--shared \
		--prefix=$(PWD)/$(INSTALL_DIR)
	mv $(@D)/Makefile $(@D)/Makefile.shared
	echo 'all $$(MAKECMDGOALS):' > $(@D)/Makefile
	echo '	make -f Makefile.static $$(MAKECMDGOALS)' >> $(@D)/Makefile
	echo '	make -f Makefile.shared $$(MAKECMDGOALS)' >> $(@D)/Makefile
# It is not possible to pass CFLAGS to zlib's configure.
MAKEVAR_OVERRIDE_ZLIB:=CFLAGS="$(_CFLAGS)"

# Don't configure GLEW.
# GLEW does not support building outside of the source tree, so just copy
# everything over (it's a small package).
$(BUILD_DIR)/$(PACKAGE_GLEW)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_GLEW)
	mkdir -p $(dir $(@D))
	rm -rf $(@D)
	cp -r $< $(@D)

# Configure Tcl.
$(BUILD_DIR)/$(PACKAGE_TCL)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_TCL)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</$(TCL_OS)/configure \
		--prefix=$(PWD)/$(INSTALL_DIR)
# Tcl's configure ignores CFLAGS passed to it.
MAKEVAR_OVERRIDE_TCL:=CFLAGS_OPTIMIZE="$(_CFLAGS)"

# Configure libxml2.
$(BUILD_DIR)/$(PACKAGE_XML)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_XML) \
  $(foreach PACKAGE,$(filter-out $(SYSTEM_LIBS),ZLIB),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$</configure \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--with-minimum \
 		--with-push \
		$(if $(filter-out $(SYSTEM_LIBS),ZLIB),--with-zlib=$(PWD)/$(INSTALL_DIR),) \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS=-L$(PWD)/$(INSTALL_DIR)/lib

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
$(foreach PACKAGE,$(PACKAGES),$(SOURCE_DIR)/$(PACKAGE_$(PACKAGE))):
	rm -rf $@
	mkdir -p $(@D)
	tar -zxf $< -C $(@D)
	if [ -n "$(EXTRACTED_NAME_$(call findpackage,TARBALL,$(<F)))" ]; then mv $(@D)/$(EXTRACTED_NAME_$(call findpackage,TARBALL,$(<F))) $@ ; fi
	test ! -e $(PATCHES_DIR)/$(PACKAGE_$(call findpackage,TARBALL,$(<F))).diff || patch -p1 -N -u -d $@ < $(PATCHES_DIR)/$(PACKAGE_$(call findpackage,TARBALL,$(<F))).diff
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
