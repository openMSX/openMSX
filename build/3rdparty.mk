# Compiles 3rd party libraries needed by openMSX.
# It enables only the features needed by openMSX.

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

.PHONY: all
all: default

# Get information about packages.
-include derived/3rdparty/packages.mk

ifneq ($(origin PACKAGE_SDL2),undefined)

# These libraries are part of the base system, therefore we do not need to
# link them statically for building a redistributable binary.
SYSTEM_LIBS:=$(shell $(PYTHON) build/list_system_libs.py $(OPENMSX_TARGET_OS))

# Compiler selection, compiler flags, SDK selection.
# These variables are already exported, but we make it explicit here.
export CC

CC=$(_CC)

TIMESTAMP_DIR:=$(BUILD_PATH)/timestamps
BUILD_DIR:=$(BUILD_PATH)/build
INSTALL_DIR:=$(BUILD_PATH)/install
TOOLS_DIR:=$(BUILD_PATH)/tools

# Create a GNU-style system tuple.
# Unfortunately, these are very poorly standardized, so our aim is to
# create tuples that are known to work, instead of using a consistent
# algorithm across all platforms.

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

ifneq ($(filter android,$(OPENMSX_TARGET_OS)),)
TRIPLE_OS:=linux
else
TRIPLE_OS:=$(OPENMSX_TARGET_OS)
endif

TRIPLE_VENDOR:=unknown
ifeq ($(OPENMSX_TARGET_OS),mingw-w64)
TRIPLE_OS:=mingw32
TRIPLE_VENDOR:=w64
endif

TARGET_TRIPLE:=$(TRIPLE_MACHINE)-$(TRIPLE_VENDOR)-$(TRIPLE_OS)

# Libraries using old autoconf macros don't recognise Android as an OS,
# so we have to tell those we're building for Linux instead.
OLD_TARGET_TRIPLE:=$(TARGET_TRIPLE)

# For all other libraries, we pass an Android-specific system tuple.
# These are triples but instead of machine-vendor-os they use machine-os-abi.
ifeq ($(OPENMSX_TARGET_OS),android)
TARGET_TRIPLE:=$(TRIPLE_MACHINE)-$(TRIPLE_OS)-android
ifeq ($(TRIPLE_MACHINE),arm)
TARGET_TRIPLE:=$(TARGET_TRIPLE)eabi
endif
endif

export PKG_CONFIG:=$(PWD)/$(TOOLS_DIR)/bin/$(TARGET_TRIPLE)-pkg-config

# Ask the compiler for the names and locations of other toolchain components.
# This works with GCC and Clang at least, so it should be pretty safe.
export LD:=$(shell $(CC) -print-prog-name=ld)
export AR:=$(shell $(CC) -print-prog-name=ar)
export RANLIB:=$(shell $(CC) -print-prog-name=ranlib)
export STRIP:=$(shell $(CC) -print-prog-name=strip)
ifeq ($(TRIPLE_OS),mingw32)
# SDL calls it WINDRES, Tcl calls it RC; provide both.
export WINDRES
export RC:=$(WINDRES)
endif

# Although X11 is available on Windows and Mac OS X, most people do not have
# it installed, so do not link against it.
ifeq ($(filter linux freebsd netbsd openbsd gnu,$(OPENMSX_TARGET_OS)),)
USE_VIDEO_X11:=disable
else
USE_VIDEO_X11:=enable
endif

ifeq ($(OPENMSX_TARGET_OS),android)
# SDL2's top-level Makefile does not build the platform glue, so linking will fail.
# As a workaround, we disable HIDAPI, but in the long term we should either fix the
# issue in SDL2 or build SDL2 using its top-level Android.mk.
USE_HIDAPI:=disable
else
USE_HIDAPI:=enable
endif

PACKAGES_BUILD:=$(shell $(PYTHON) build/3rdparty_libraries.py $(OPENMSX_TARGET_OS) $(LINK_MODE)) PKG_CONFIG
PACKAGES_NOBUILD:=
PACKAGES_3RD:=$(PACKAGES_BUILD) $(PACKAGES_NOBUILD)

# Function which, given a variable name prefix and the variable's value,
# returns the name of the package.
findpackage=$(strip $(foreach PACKAGE,$(PACKAGES_3RD),$(if $(filter $(2),$($(1)_$(PACKAGE))),$(PACKAGE),)))

# Function which, given a list of packages, produces a list of the install
# timestamps for those packages.
installdeps=$(foreach PACKAGE,$(1),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))

BUILD_TARGETS:=$(foreach PACKAGE,$(PACKAGES_BUILD),$(TIMESTAMP_DIR)/build-$(PACKAGE_$(PACKAGE)))
INSTALL_BUILD_TARGETS:=$(call installdeps,$(PACKAGES_BUILD))
INSTALL_NOBUILD_TARGETS:=$(call installdeps,$(PACKAGES_NOBUILD))

INSTALL_PARAMS_GLEW:=\
	GLEW_DEST=$(PWD)/$(INSTALL_DIR) \
	LIBDIR=$(PWD)/$(INSTALL_DIR)/lib

.PHONY: default
default: $(INSTALL_BUILD_TARGETS) $(INSTALL_NOBUILD_TARGETS)

.PHONY: clean
clean:
	rm -rf $(SOURCE_DIR)
	rm -rf $(BUILD_DIR)
	rm -rf $(INSTALL_DIR)

# Install.
$(INSTALL_BUILD_TARGETS): $(TIMESTAMP_DIR)/install-%: $(TIMESTAMP_DIR)/build-%
	$(MAKE) -C $(BUILD_DIR)/$* install \
		$(MAKEVAR_OVERRIDE_$(call findpackage,PACKAGE,$*)) \
		$(INSTALL_PARAMS_$(call findpackage,PACKAGE,$*))
	mkdir -p $(@D)
	touch $@

# Build.
$(BUILD_TARGETS): $(TIMESTAMP_DIR)/build-%: $(BUILD_DIR)/%/Makefile
	$(MAKE) -C $(<D) $(MAKEVAR_OVERRIDE_$(call findpackage,PACKAGE,$*))
	mkdir -p $(@D)
	touch $@

# Configure pkg-config.
$(BUILD_DIR)/$(PACKAGE_PKG_CONFIG)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_PKG_CONFIG)/.extracted
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--with-internal-glib \
		--disable-host-tool \
		--program-prefix=$(TARGET_TRIPLE)- \
		--prefix=$(PWD)/$(TOOLS_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		CFLAGS="-Wno-error=int-conversion" \
		CC= LD= AR= RANLIB= STRIP=

# Configure SDL2.
$(BUILD_DIR)/$(PACKAGE_SDL2)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL2)/.extracted \
  $(call installdeps,PKG_CONFIG)
	mkdir -p $(@D)
	cd $(@D) && \
		$(PWD)/$(<D)/configure \
		--$(USE_HIDAPI)-hidapi \
		--$(USE_VIDEO_X11)-video-x11 \
		--disable-video-directfb \
		--disable-video-opengles1 \
		--disable-nas \
		--disable-esd \
		--disable-arts \
		--disable-shared \
		$(if $(filter %clang,$(CC)),--disable-arm-simd,) \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) -L$(PWD)/$(INSTALL_DIR)/lib"
# Some modules are enabled because of internal SDL2 dependencies:
# - "audio" depends on "atomic" and "threads"
# - "joystick" depends on "haptic" (at least in the Windows back-end)
# - OpenGL on Windows depends on "loadso"

# Configure SDL2_ttf.
$(BUILD_DIR)/$(PACKAGE_SDL2_TTF)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL2_TTF)/.extracted \
  $(call installdeps,PKG_CONFIG SDL2 FREETYPE)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-sdltest \
		--disable-shared \
		--disable-freetype-builtin \
		--disable-harfbuzz \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--$(subst disable,without,$(subst enable,with,$(USE_VIDEO_X11)))-x \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS)"
# Disable building of example programs.
# This build fails on Android (SDL main issues), but on other platforms
# we don't need these programs either.
MAKEVAR_OVERRIDE_SDL2_TTF:=noinst_PROGRAMS=""

# Configure libpng.
$(BUILD_DIR)/$(PACKAGE_PNG)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_PNG)/.extracted \
  $(call installdeps,$(filter-out $(SYSTEM_LIBS),ZLIB))
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) -L$(PWD)/$(INSTALL_DIR)/lib"

# Configure FreeType.
$(BUILD_DIR)/$(PACKAGE_FREETYPE)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_FREETYPE)/.extracted \
  $(call installdeps,PKG_CONFIG)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-shared \
		--without-zlib \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)"

# Configure zlib.
# Although it uses "configure", zlib does not support building outside of the
# source tree, so just copy everything over (it's a small package).
$(BUILD_DIR)/$(PACKAGE_ZLIB)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_ZLIB)/.extracted
	mkdir -p $(dir $(@D))
	rm -rf $(@D)
	cp -r $(<D) $(@D)
	cd $(@D) && ./configure \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--static
# It is not possible to pass CFLAGS to zlib's configure.
MAKEVAR_OVERRIDE_ZLIB:=CFLAGS="$(_CFLAGS)"
# Note: zlib's Makefile uses LDFLAGS to link its examples, not the library
#       itself. If we mess with it, the build breaks.

# Don't configure GLEW.
# GLEW does not support building outside of the source tree, so just copy
# everything over (it's a small package).
$(BUILD_DIR)/$(PACKAGE_GLEW)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_GLEW)/.extracted
	mkdir -p $(dir $(@D))
	rm -rf $(@D)
	cp -r $(<D) $(@D)
# GLEW does not have a configure script to pass CFLAGS to.
MAKEVAR_OVERRIDE_GLEW:=CC="$(_CC) $(_CFLAGS)" LD="$(_CC) $(_LDFLAGS)"
# Tell GLEW to cross compile.
ifeq ($(TRIPLE_OS),mingw32)
MAKEVAR_OVERRIDE_GLEW+=SYSTEM=linux-mingw64
else
MAKEVAR_OVERRIDE_GLEW+=SYSTEM=$(TRIPLE_OS)
endif

# Configure Tcl.
# Note: Tcl 8.6 includes some bundled extensions. We don't want these and there
#       is no configure flag to disable them, so we remove the pkgs/ directory
#       that they are in.
# Note: Tcl seems to build either dynamic libs or static libs, which is why we
#       have to pass --disable-shared to configure.
ifeq ($(TRIPLE_OS),mingw32)
TCL_OS:=win
else
# Note: Use "unix" on Mac OS X as well. There is a "configure" script in
#       the "macosx" dir, but that is only intended for use with Xcode.
TCL_OS:=unix
endif
CONFIGURE_OVERRIDE_TCL:=
ifeq ($(OPENMSX_TARGET_OS),android)
# nl_langinfo was introduced in API version 26, but for some reason configure
# detects it on older API versions as well.
CONFIGURE_OVERRIDE_TCL+=--disable-langinfo
# The structs for 64-bit file offsets are already present in API version 14,
# but the functions were only added in version 21. Tcl assumes that if the
# structs are present, the functions are also present. So we override the
# detection of the structs.
CONFIGURE_OVERRIDE_TCL+=tcl_cv_struct_dirent64=no tcl_cv_struct_stat64=no
endif
$(BUILD_DIR)/$(PACKAGE_TCL)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_TCL)/.extracted
	mkdir -p $(@D)
	rm -rf $(SOURCE_DIR)/$(PACKAGE_TCL)/pkgs/
	cd $(@D) && $(PWD)/$(<D)/$(TCL_OS)/configure \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--disable-shared \
		--without-tzdata \
		$(CONFIGURE_OVERRIDE_TCL) \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)"

# Configure Ogg, Vorbis and Theora for Laserdisc emulation.

$(BUILD_DIR)/$(PACKAGE_OGG)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_OGG)/.extracted \
  $(call installdeps,PKG_CONFIG)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)"

$(BUILD_DIR)/$(PACKAGE_VORBIS)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_VORBIS)/.extracted \
  $(call installdeps,PKG_CONFIG OGG)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-shared \
		--disable-oggtest \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--with-ogg=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)"

# Note: According to its spec file, Theora has a build dependency on both
#       Ogg and Vorbis, a runtime dependency on Vorbis and a development
#       package dependency on Ogg.
$(BUILD_DIR)/$(PACKAGE_THEORA)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_THEORA)/.extracted \
  $(call installdeps,PKG_CONFIG OGG VORBIS)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-shared \
		--disable-oggtest \
		--disable-vorbistest \
		--disable-sdltest \
		--disable-encode \
		--disable-examples \
		--host=$(OLD_TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--with-ogg=$(PWD)/$(INSTALL_DIR) \
		--with-vorbis=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)"

endif

# Rules for creating and updating generated Makefiles:

derived/3rdparty/packages.mk: \
  build/3rdparty_packages2make.py build/packages.py
	mkdir -p $(@D)
	$(PYTHON) $< > $@
