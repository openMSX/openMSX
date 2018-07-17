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

ifneq ($(origin PACKAGE_SDL),undefined)

# These libraries are part of the base system, therefore we do not need to
# link them statically for building a redistributable binary.
SYSTEM_LIBS:=$(shell $(PYTHON) build/list_system_libs.py $(OPENMSX_TARGET_OS))

# Compiler selection, compiler flags, SDK selection.
# These variables are already exported, but we make it explicit here.
export CC
export NEXT_ROOT

CC=$(_CC)

TIMESTAMP_DIR:=$(BUILD_PATH)/timestamps
BUILD_DIR:=$(BUILD_PATH)/build
INSTALL_DIR:=$(BUILD_PATH)/install

# Create a GNU-style system triple.
TRIPLE_VENDOR:=unknown
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
ifneq ($(filter android dingux,$(OPENMSX_TARGET_OS)),)
TRIPLE_OS:=linux
else
TRIPLE_OS:=$(OPENMSX_TARGET_OS)
endif
ifeq ($(OPENMSX_TARGET_OS),mingw-w64)
TRIPLE_OS:=mingw32
TRIPLE_VENDOR:=w64
endif
TARGET_TRIPLE:=$(TRIPLE_MACHINE)-$(TRIPLE_VENDOR)-$(TRIPLE_OS)

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

# Work around some autoconf versions returning "universal" for endianess when
# compiling with "-arch" in the CFLAGS, even in a single arch compile.
BIGENDIAN:=$(shell PYTHONPATH=build/ $(PYTHON) -c 'from cpu import getCPU ; print "yes" if getCPU("$(OPENMSX_TARGET_CPU)").bigEndian else "no"')
ifeq ($(BIGENDIAN),)
$(error Could not determine endianess of "$(OPENMSX_TARGET_CPU)")
endif

# Although X11 is available on Windows and Mac OS X, most people do not have
# it installed, so do not link against it.
ifeq ($(filter linux freebsd netbsd openbsd gnu,$(OPENMSX_TARGET_OS)),)
USE_VIDEO_X11:=disable
else
USE_VIDEO_X11:=enable
endif

PACKAGES_BUILD:=$(shell $(PYTHON) build/3rdparty_libraries.py $(OPENMSX_TARGET_OS) $(LINK_MODE))
PACKAGES_NOBUILD:=
PACKAGES_3RD:=$(PACKAGES_BUILD) $(PACKAGES_NOBUILD)

BUILD_TARGETS:=$(foreach PACKAGE,$(PACKAGES_BUILD),$(TIMESTAMP_DIR)/build-$(PACKAGE_$(PACKAGE)))
INSTALL_BUILD_TARGETS:=$(foreach PACKAGE,$(PACKAGES_BUILD),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))
INSTALL_NOBUILD_TARGETS:=$(foreach PACKAGE,$(PACKAGES_NOBUILD),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))

INSTALL_PARAMS_GLEW:=\
	GLEW_DEST=$(PWD)/$(INSTALL_DIR) \
	LIBDIR=$(PWD)/$(INSTALL_DIR)/lib

# Function which, given a variable name prefix and the variable's value,
# returns the name of the package.
findpackage=$(strip $(foreach PACKAGE,$(PACKAGES_3RD),$(if $(filter $(2),$($(1)_$(PACKAGE))),$(PACKAGE),)))

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

# Configuration of a lib can depend on the lib-config script of another lib.
PNG_CONFIG_SCRIPT:=$(INSTALL_DIR)/bin/libpng-config
$(PNG_CONFIG_SCRIPT): $(TIMESTAMP_DIR)/install-$(PACKAGE_PNG)
FREETYPE_CONFIG_SCRIPT:=$(INSTALL_DIR)/bin/freetype-config
$(FREETYPE_CONFIG_SCRIPT): $(TIMESTAMP_DIR)/install-$(PACKAGE_FREETYPE)
ifeq ($(OPENMSX_TARGET_OS),android)
SDL_CONFIG_SCRIPT:=$(SDL_ANDROID_PORT_PATH)/project/jni/application/sdl-config
else
SDL_CONFIG_SCRIPT:=$(INSTALL_DIR)/bin/sdl-config
$(SDL_CONFIG_SCRIPT): $(TIMESTAMP_DIR)/install-$(PACKAGE_SDL)
endif

# Configure ALSA.
$(BUILD_DIR)/$(PACKAGE_ALSA)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_ALSA)/.extracted
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--enable-static \
		--disable-shared \
		--enable-symbolic-functions \
		--disable-debug \
		--disable-aload \
		--disable-mixer \
		--enable-pcm \
		--disable-rawmidi \
		--disable-hwdep \
		--enable-seq \
		--disable-ucm \
		--disable-topology \
		--disable-alisp \
		--disable-old-symbols \
		--disable-python \
		--with-debug=no \
		--with-libdl=no \
		--with-pthread=yes \
		--with-librt=yes \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) -L$(PWD)/$(INSTALL_DIR)/lib"

# Configure SDL.
$(BUILD_DIR)/$(PACKAGE_SDL)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL)/.extracted \
  $(foreach PACKAGE,$(filter ALSA,$(PACKAGES_3RD)),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))
	mkdir -p $(@D)
	cd $(@D) && \
		ac_cv_c_bigendian=$(BIGENDIAN) \
		$(PWD)/$(<D)/configure \
		--$(USE_VIDEO_X11)-video-x11 \
		--disable-video-dga \
		--disable-video-directfb \
		--disable-video-svga \
		--disable-nas \
		--disable-esd \
		--disable-directx \
		--disable-cdrom \
		--disable-stdio-redirect \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS) -L$(PWD)/$(INSTALL_DIR)/lib"
# While openMSX does not use "cpuinfo", "endian" and "file" modules, other
# modules do and if we disable them, SDL will not link.

# Configure SDL_ttf.
$(BUILD_DIR)/$(PACKAGE_SDL_TTF)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_SDL_TTF)/.extracted \
  $(FREETYPE_CONFIG_SCRIPT) $(SDL_CONFIG_SCRIPT)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-sdltest \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--$(subst disable,without,$(subst enable,with,$(USE_VIDEO_X11)))-x \
		ac_cv_path_FREETYPE_CONFIG=$(abspath $(FREETYPE_CONFIG_SCRIPT)) \
		ac_cv_path_SDL_CONFIG=$(abspath $(SDL_CONFIG_SCRIPT)) \
		CFLAGS="$(_CFLAGS)" \
		CPPFLAGS="-I$(PWD)/$(INSTALL_DIR)/include" \
		LDFLAGS="$(_LDFLAGS)" \
		PKG_CONFIG=/nowhere
# Disable building of example programs.
# This build fails on Android (SDL main issues), but on other platforms
# we don't need these programs either.
MAKEVAR_OVERRIDE_SDL_TTF:=noinst_PROGRAMS=""

# Configure libpng.
$(BUILD_DIR)/$(PACKAGE_PNG)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_PNG)/.extracted \
  $(foreach PACKAGE,$(filter-out $(SYSTEM_LIBS),ZLIB),$(TIMESTAMP_DIR)/install-$(PACKAGE_$(PACKAGE)))
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
  $(SOURCE_DIR)/$(PACKAGE_FREETYPE)/.extracted
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
MAKEVAR_OVERRIDE_GLEW+=SYSTEM=mingw
else
MAKEVAR_OVERRIDE_GLEW+=SYSTEM=$(TRIPLE_OS)
endif

# Configure Tcl.
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
	cd $(@D) && $(PWD)/$(<D)/$(TCL_OS)/configure \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--disable-shared \
		--disable-threads \
		--without-tzdata \
		$(CONFIGURE_OVERRIDE_TCL) \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)"

# Configure Ogg, Vorbis and Theora for Laserdisc emulation.

$(BUILD_DIR)/$(PACKAGE_OGG)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_OGG)/.extracted
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-shared \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)" \
		PKG_CONFIG=/nowhere

$(BUILD_DIR)/$(PACKAGE_VORBIS)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_VORBIS)/.extracted \
  $(TIMESTAMP_DIR)/install-$(PACKAGE_OGG)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-shared \
		--disable-oggtest \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--with-ogg=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)" \
		PKG_CONFIG=/nowhere

# Note: According to its spec file, Theora has a build dependency on both
#       Ogg and Vorbis, a runtime dependency on Vorbis and a development
#       package dependency on Ogg.
$(BUILD_DIR)/$(PACKAGE_THEORA)/Makefile: \
  $(SOURCE_DIR)/$(PACKAGE_THEORA)/.extracted \
  $(TIMESTAMP_DIR)/install-$(PACKAGE_OGG) \
  $(TIMESTAMP_DIR)/install-$(PACKAGE_VORBIS)
	mkdir -p $(@D)
	cd $(@D) && $(PWD)/$(<D)/configure \
		--disable-shared \
		--disable-oggtest \
		--disable-vorbistest \
		--disable-sdltest \
		--disable-encode \
		--disable-examples \
		--host=$(TARGET_TRIPLE) \
		--prefix=$(PWD)/$(INSTALL_DIR) \
		--libdir=$(PWD)/$(INSTALL_DIR)/lib \
		--with-ogg=$(PWD)/$(INSTALL_DIR) \
		--with-vorbis=$(PWD)/$(INSTALL_DIR) \
		CFLAGS="$(_CFLAGS)" \
		LDFLAGS="$(_LDFLAGS)" \
		PKG_CONFIG=/nowhere

endif

# Rules for creating and updating generated Makefiles:

derived/3rdparty/packages.mk: \
  build/3rdparty_packages2make.py build/packages.py
	mkdir -p $(@D)
	$(PYTHON) $< > $@
