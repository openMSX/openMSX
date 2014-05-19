# openMSX Build System
# ====================
#
# This is the home made build system for openMSX, which replaced the
# autoconf/automake combo.
#
# Used a lot of ideas from Peter Miller's excellent paper
# "Recursive Make Considered Harmful".
# http://miller.emu.id.au/pmiller/books/rmch/

# TODO:
# - Move calculation of CFLAGS and LDFLAGS to components2defs.py?


# Python Interpreter
# ==================

# We need Python from the 2.x series, version 2.6 or higher.
# Usually this executable is available as just "python", but on some systems
# you might have to be more specific, for example "python2" or "python2.6".
# Or if the Python interpreter is not in the search path, you can specify its
# full path.
ifeq ($(PYTHON),)
PYTHON:=$(shell build/python-search.sh)
ifeq ($(PYTHON),)
$(error No suitable Python interpreter found. Please install Python version 2.x where x >= 5. If your Python interpreter is installed in a non-standard location, please set the environment variable PYTHON to the full path of the interpreter binary.)
endif
endif
$(info Using Python: $(PYTHON))


# Delete on Error
# ===============

# Delete output if rule fails.
# This is a flag that applies to all rules.
.DELETE_ON_ERROR:


# Logical Targets
# ===============

ifneq ($(words $(MAKECMDGOALS)),1)
$(error main.mk can only handle once goal at a time)
endif

# TODO: "dist" and "createsubs" are missing
# TODO: more missing?
# Logical targets which require dependency files.
DEPEND_TARGETS:=all default install run bindist
# Logical targets which do not require dependency files.
NODEPEND_TARGETS:=clean config probe 3rdparty staticbindist
# Mark all logical targets as such.
.PHONY: $(DEPEND_TARGETS) $(NODEPEND_TARGETS)


# Settings
# ========
#
# There are platform specific settings and flavour specific settings.
#   platform: architecture, OS
#   flavour:  optimisation levels, debugging, profiling

# Function to check a variable has been defined and has a non-empty value.
# Usage: $(call DEFCHECK,VARIABLE_NAME)
DEFCHECK=$(strip \
	$(if $(filter _undefined,_$(origin $(1))), \
		$(error Variable $(1) is undefined) ) \
	)

# Function to check a boolean variable has value "true" or "false".
# Usage: $(call BOOLCHECK,VARIABLE_NAME)
BOOLCHECK=$(DEFCHECK)$(strip \
	$(if $(filter-out _true _false,_$($(1))), \
		$(error Value of $(1) ("$($(1))") should be "true" or "false") ) \
	)

# Will be added to by platform specific Makefile, by flavour specific Makefile
# and by this Makefile.
# Note: CXXFLAGS is overridable from the command line; COMPILE_FLAGS is not.
#       We use CXXFLAGS for flavour specific flags and COMPILE_FLAGS for
#       platform specific flags.
CXXFLAGS:=
COMPILE_FLAGS:=
COMPILE_ENV:=
# Note: LDFLAGS are passed to the linker itself, LINK_FLAGS are passed to the
#       compiler in the link phase.
LDFLAGS:=
LINK_FLAGS:=
LINK_ENV:=
# Flags that specify the target platform.
# These should be inherited by the 3rd party libs Makefile.
TARGET_FLAGS:=


# Customisation
# =============

include build/custom.mk
$(call DEFCHECK,INSTALL_BASE)
$(call BOOLCHECK,VERSION_EXEC)
$(call BOOLCHECK,SYMLINK_FOR_BINARY)
$(call BOOLCHECK,INSTALL_CONTRIB)


# Platforms
# =========

# Note:
# A platform currently specifies both the host platform (performing the build)
# and the target platform (running the created binary). When we have real
# experience with cross-compilation, a more sophisticated system can be
# designed.

LINK_MODE:=$(if $(filter true,$(3RDPARTY_FLAG)),3RD_STA,SYS_DYN)

# Do not perform autodetection if platform was specified by the user.
ifneq ($(filter undefined,$(origin OPENMSX_TARGET_CPU) $(origin OPENMSX_TARGET_OS)),)
DETECTSYS_SCRIPT:=build/detectsys.py
LOCAL_PLATFORM:=$(shell $(PYTHON) $(DETECTSYS_SCRIPT))
ifeq ($(LOCAL_PLATFORM),)
$(error No platform specified using OPENMSX_TARGET_CPU and OPENMSX_TARGET_OS and autodetection of local platform failed)
endif
OPENMSX_TARGET_CPU:=$(word 1,$(subst -, ,$(LOCAL_PLATFORM)))
OPENMSX_TARGET_OS:=$(word 2,$(subst -, ,$(LOCAL_PLATFORM)))
endif # OPENMSX_TARGET_CPU && OPENMSX_TARGET_OS

PLATFORM:=
ifneq ($(origin OPENMSX_TARGET_OS),undefined)
ifneq ($(origin OPENMSX_TARGET_CPU),undefined)
PLATFORM:=$(OPENMSX_TARGET_CPU)-$(OPENMSX_TARGET_OS)
endif
endif

# Ignore rest of Makefile if autodetection was not performed yet.
# Note that the include above will force a reload of the Makefile.
ifneq ($(PLATFORM),)

# List of CPUs to compile for.
ifeq ($(OPENMSX_TARGET_CPU),univ)
CPU_LIST:=x86 x86_64
else
CPU_LIST:=$(OPENMSX_TARGET_CPU)
endif

# Default flavour.
$(call DEFCHECK,OPENMSX_TARGET_CPU)
ifeq ($(OPENMSX_TARGET_CPU),x86)
ifeq ($(filter darwin%,$(OPENMSX_TARGET_OS)),)
# To run openMSX with decent speed, at least a Pentium 2 class machine
# is needed, so let's optimise for that.
OPENMSX_FLAVOUR?=i686
else
# The system headers of OS X use SSE features, which are not available on
# i686, so we only use the generic optimisation flags instead.
OPENMSX_FLAVOUR?=opt
endif
else
ifeq ($(OPENMSX_TARGET_CPU),ppc)
OPENMSX_FLAVOUR?=ppc
else
ifeq ($(OPENMSX_TARGET_CPU),m68k)
OPENMSX_FLAVOUR?=m68k
else
OPENMSX_FLAVOUR?=opt
endif
endif
endif

# Load OS specific settings.
$(call DEFCHECK,OPENMSX_TARGET_OS)
include build/platform-$(OPENMSX_TARGET_OS).mk
# Check that all expected variables were defined by OS specific Makefile:
# - library file name extension
$(call DEFCHECK,LIBRARYEXT)
# - executable file name extension
$(call DEFCHECK,EXEEXT)
# - platform supports symlinks?
$(call BOOLCHECK,USE_SYMLINK)

ifneq ($(OPENMSX_TARGET_CPU),univ)
# Get CPU specific flags.
TARGET_FLAGS+=$(shell $(PYTHON) build/cpu2flags.py $(OPENMSX_TARGET_CPU))
endif


# Flavours
# ========

ifneq ($(OPENMSX_TARGET_CPU),univ)
# Load flavour specific settings.
include build/flavour-$(OPENMSX_FLAVOUR).mk
endif


# Paths
# =====

BUILD_PATH:=derived/$(PLATFORM)-$(OPENMSX_FLAVOUR)
ifeq ($(3RDPARTY_FLAG),true)
  BUILD_PATH:=$(BUILD_PATH)-3rd
endif

# Own build of 3rd party libs.
ifeq ($(3RDPARTY_FLAG),true)
3RDPARTY_INSTALL_DIR:=$(BUILD_PATH)/3rdparty/install
endif

SOURCES_PATH:=src

BINARY_PATH:=$(BUILD_PATH)/bin
BINARY_FILE:=openmsx$(EXEEXT)

BINDIST_DIR:=$(BUILD_PATH)/bindist
BINDIST_PACKAGE:=

ifeq ($(VERSION_EXEC),true)
  REVISION:=$(shell PYTHONPATH=build $(PYTHON) -c \
    "import version; print version.extractRevisionString()" \
    )
  BINARY_FULL:=$(BINARY_PATH)/openmsx-$(REVISION)$(EXEEXT)
else
  BINARY_FULL:=$(BINARY_PATH)/$(BINARY_FILE)
endif

LIBRARY_FILE:=openmsx$(LIBRARYEXT)
LIBRARY_PATH:=$(BUILD_PATH)/lib
LIBRARY_FULL:=$(LIBRARY_PATH)/$(LIBRARY_FILE)

BUILDINFO_SCRIPT:=build/buildinfo2code.py
CONFIG_HEADER:=$(BUILD_PATH)/config/build-info.hh
PROBE_SCRIPT:=build/probe.py
PROBE_MAKE:=$(BUILD_PATH)/config/probed_defs.mk
VERSION_SCRIPT:=build/version2code.py
VERSION_HEADER:=$(BUILD_PATH)/config/Version.ii
COMPONENTS_HEADER_SCRIPT:=build/components2code.py
COMPONENTS_DEFS_SCRIPT:=build/components2defs.py
COMPONENTS_HEADER:=$(BUILD_PATH)/config/components.hh
COMPONENTS_DEFS:=$(BUILD_PATH)/config/components_defs.mk
GENERATED_HEADERS:=$(VERSION_HEADER) $(CONFIG_HEADER) $(COMPONENTS_HEADER)


# Configuration
# =============

ifneq ($(OPENMSX_TARGET_CPU),univ)
ifneq ($(filter $(DEPEND_TARGETS),$(MAKECMDGOALS)),)
-include $(PROBE_MAKE)
-include $(COMPONENTS_DEFS)
endif # goal requires dependencies
endif # universal binary


# Filesets
# ========

SOURCE_DIRS:=$(sort $(shell find src -type d))

SOURCES_FULL:=$(foreach dir,$(SOURCE_DIRS),$(sort $(wildcard $(dir)/*.cc)))
SOURCES_FULL:=$(filter-out %Test.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out src/sound/generate%.cc,$(SOURCES_FULL))

# TODO: This doesn't work since MAX_SCALE_FACTOR is not a Make variable,
#       only a #define in build-info.hh.
ifeq ($(MAX_SCALE_FACTOR),1)
define SOURCES_UPSCALE
	Scanline
	Scaler2 Scaler3
	Simple2xScaler Simple3xScaler
	SaI2xScaler SaI3xScaler
	Scale2xScaler Scale3xScaler
	HQ2xScaler HQ2xLiteScaler
	HQ3xScaler HQ3xLiteScaler
	RGBTriplet3xScaler MLAAScaler
	Multiply32
endef
SOURCES_FULL:=$(filter-out $(foreach src,$(strip $(SOURCES_UPSCALE)),src/video/scalers/$(src).cc),$(SOURCES_FULL))
endif

ifneq ($(COMPONENT_GL),true)
SOURCES_FULL:=$(filter-out src/video/GL%.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out src/video/SDLGL%.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out src/video/scalers/GL%.cc,$(SOURCES_FULL))
endif

ifneq ($(COMPONENT_LASERDISC),true)
SOURCES_FULL:=$(filter-out src/laserdisc/%.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out src/video/ld/%.cc,$(SOURCES_FULL))
endif

SOURCES:=$(SOURCES_FULL:$(SOURCES_PATH)/%.cc=%)

DEPEND_PATH:=$(BUILD_PATH)/dep
DEPEND_FULL:=$(addsuffix .d,$(addprefix $(DEPEND_PATH)/,$(SOURCES)))

OBJECTS_PATH:=$(BUILD_PATH)/obj
OBJECTS_FULL:=$(addsuffix .o,$(addprefix $(OBJECTS_PATH)/,$(SOURCES)))

ifneq ($(filter mingw%,$(OPENMSX_TARGET_OS)),)
RESOURCE_SRC:=src/resource/openmsx.rc
RESOURCE_OBJ:=$(OBJECTS_PATH)/resources.o
RESOURCE_SCRIPT:=build/win_resource.py
RESOURCE_HEADER:=$(BUILD_PATH)/config/resource-info.h
else
RESOURCE_OBJ:=
endif


# Compiler and Flags
# ==================

COMPILE_FLAGS+=$(TARGET_FLAGS)
LINK_FLAGS+=$(TARGET_FLAGS)

# Determine compiler.
CXX?=g++
WINDRES?=windres
DEPEND_FLAGS:=
ifneq ($(filter %clang++,$(CXX))$(filter clang++%,$(CXX)),)
  # Enable C++11
  COMPILE_FLAGS+=-std=c++11
  # Clang does support -Wunused-macros, but it triggers on SDL's headers,
  # causing way too many false positives that we cannot fix.
  COMPILE_FLAGS+=-Wall -Wextra -Wundef
  # TODO: Remove the overloading from the code instead.
  COMPILE_FLAGS+=-Wno-overloaded-virtual
  CC:=$(subst clang++,clang,$(CXX))
  LD:=ld
  DEPEND_FLAGS+=-MP
else
ifneq ($(filter %g++,$(CXX))$(filter g++%,$(CXX))$(findstring /g++-,$(CXX)),)
  # Generic compilation flags.
  COMPILE_FLAGS+=-pipe
  # Enable C++11
  COMPILE_FLAGS+=-std=gnu++0x
  # Stricter warning and error reporting.
  COMPILE_FLAGS+=-Wall -Wextra -Wundef -Wunused-macros
  # -Wdouble-promotion <-- this is useful, but only support from gcc-4.6
  # Flag that is not accepted by old GCC versions.
  COMPILE_FLAGS+=$(shell \
    echo | $(CXX) -E -Wno-missing-field-initializers - >/dev/null 2>&1 \
    && echo -Wno-missing-field-initializers \
    )

  # -Wzero-as-null-pointer-constant is available from gcc-4.7
  ## IMHO this is a useful but not very important warning. It triggers in
  ## quite a few places via macros defined in tcl8.5/tclDecls.h, so we can't
  ## easily suppress it. So for now I'll disable this warning again.
  ##COMPILE_FLAGS+=$(shell \
  ##  echo | $(CXX) -E -Wzero-as-null-pointer-constant - >/dev/null 2>&1 \
  ##  && echo -Wzero-as-null-pointer-constant \
  ##  )

  # Empty definition of used headers, so header removal doesn't break things.
  DEPEND_FLAGS+=-MP
  # Plain C compiler, for the 3rd party libs.
  CC:=$(subst g++,gcc,$(CXX))
  # Guess the name of the linker.
  LD:=$(subst g++,ld,$(CXX:g++%=g++))
else
  ifneq ($(filter %gcc,$(CXX))$(filter gcc%,$(CXX)),)
    $(error Set CXX to your "g++" executable instead of "gcc")
  else
    $(warning Unsupported compiler: $(CXX), please update Makefile)
  endif
endif
endif

# Strip binary?
OPENMSX_STRIP?=false
$(call BOOLCHECK,OPENMSX_STRIP)
STRIP_SEPARATE:=false
ifeq ($(OPENMSX_STRIP),true)
  ifeq ($(OPENMSX_TARGET_OS),darwin)
    # Current (mid-2006) GCC 4.x for OS X will strip too many symbols,
    # resulting in a binary that cannot run.
    # However, the separate "strip" tool does work correctly.
    STRIP_SEPARATE:=true
  else
    # Tell GCC to produce a stripped binary.
    LINK_FLAGS+=-s
  endif
endif

# Determine common compile flags.
COMPILE_FLAGS+=$(addprefix -I,$(SOURCE_DIRS) $(BUILD_PATH)/config)

# Determine common link flags.
LINK_FLAGS_PREFIX:=-Wl,
LINK_FLAGS+=$(addprefix $(LINK_FLAGS_PREFIX),$(LDFLAGS))

# Determine component specific compile and link flags.
ifeq ($(COMPONENT_CORE),true)
COMPILE_FLAGS+=$(foreach lib,$(CORE_LIBS),$($(lib)_CFLAGS))
LINK_FLAGS+=$(foreach lib,$(CORE_LIBS),$($(lib)_LDFLAGS))
endif
ifeq ($(COMPONENT_GL),true)
COMPILE_FLAGS+=$(GL_CFLAGS) $(GLEW_CFLAGS) $(GLEW_GL_CFLAGS)
LINK_FLAGS+=$(GL_LDFLAGS) $(GLEW_LDFLAGS)
endif
ifeq ($(COMPONENT_LASERDISC),true)
COMPILE_FLAGS+=$(OGG_CFLAGS) $(VORBIS_CFLAGS) $(THEORA_CFLAGS)
LINK_FLAGS+=$(OGG_LDFLAGS) $(VORBIS_LDFLAGS) $(THEORA_LDFLAGS)
endif


# Build Rules
# ===========

# Force a probe if "probe" target is passed explicitly.
ifeq ($(MAKECMDGOALS),probe)
probe: $(PROBE_MAKE)
.PHONY: $(PROBE_MAKE)
endif

# Probe for headers and functions.
$(PROBE_MAKE): $(PROBE_SCRIPT) build/custom.mk \
		build/systemfuncs2code.py build/systemfuncs.py
	@$(PYTHON) $(PROBE_SCRIPT) \
		"$(COMPILE_ENV) $(CXX) $(TARGET_FLAGS)" \
		$(@D) $(OPENMSX_TARGET_OS) $(LINK_MODE) "$(3RDPARTY_INSTALL_DIR)"
	@touch $@

# Generate configuration header.
# TODO: One platform file may include another, so the real solution would be
#       for the Python script to write dependency info.
$(CONFIG_HEADER): $(BUILDINFO_SCRIPT) \
		build/custom.mk build/platform-$(OPENMSX_TARGET_OS).mk
	@$(PYTHON) $(BUILDINFO_SCRIPT) $@ \
		$(OPENMSX_TARGET_OS) $(OPENMSX_TARGET_CPU) $(OPENMSX_FLAVOUR) \
		$(INSTALL_SHARE_DIR)
	@touch $@

# Generate version header.
.PHONY: forceversionextraction
forceversionextraction:
$(VERSION_HEADER): forceversionextraction
	@$(PYTHON) $(VERSION_SCRIPT) $@

# Generate components header.
$(COMPONENTS_HEADER): $(COMPONENTS_HEADER_SCRIPT) $(PROBE_MAKE) \
		build/components.py
	@$(PYTHON) $(COMPONENTS_HEADER_SCRIPT) $@ $(PROBE_MAKE)
	@touch $@

# Generate components Makefile.
$(COMPONENTS_DEFS): $(COMPONENTS_DEFS_SCRIPT) $(PROBE_MAKE) \
		build/components.py
	@$(PYTHON) $(COMPONENTS_DEFS_SCRIPT) $@ $(PROBE_MAKE)
	@touch $@

# Default target.
ifeq ($(OPENMSX_TARGET_OS),darwin)
all: app
else
ifeq ($(OPENMSX_TARGET_OS),android)
all: $(LIBRARY_FULL)
else
all: $(BINARY_FULL)
endif
endif

# This is a workaround for the lack of order-only dependencies in GNU Make
# versions older than 3.80 (for example Mac OS X 10.3 still ships with 3.79).
# It creates a dummy file, which is never modified after its initial creation.
# If a rule that produces a file does not modify that file, Make considers the
# target to be up-to-date. That way, the targets "init-dummy-file" depends on
# will always be checked before compilation, but they will not cause all object
# files to be considered outdated.
INIT_DUMMY_FILE:=$(BUILD_PATH)/config/init-dummy-file
$(INIT_DUMMY_FILE): config $(GENERATED_HEADERS)
	@touch -a $@

# Print configuration.
config:
ifeq ($(COMPONENT_CORE),false)
# Do not build if core component dependencies are not met.
	@echo 'Cannot build openMSX because essential libraries are unavailable.'
	@echo 'Please install the needed libraries and their header files and rerun "configure"'
	@false
endif
	@echo "Build configuration:"
	@echo "  Platform: $(PLATFORM)"
	@echo "  Flavour:  $(OPENMSX_FLAVOUR)"
	@echo "  Compiler: $(CXX)"
	@echo "  Subset:   $(if $(OPENMSX_SUBSET),$(OPENMSX_SUBSET)*,full build)"

# Include dependency files.
ifneq ($(filter $(DEPEND_TARGETS),$(MAKECMDGOALS)),)
  -include $(DEPEND_FULL)
endif

# Clean up build tree of current flavour.
clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_PATH)

# Create Makefiles in source subdirectories, to conveniently build a subset.
ifeq ($(MAKECMDGOALS),createsubs)
# Function that concatenates list items to form a single string.
# Usage: $(call JOIN,TEXT)
JOIN=$(if $(1),$(firstword $(1))$(call JOIN,$(wordlist 2,999999,$(1))),)

RELPATH=$(call JOIN,$(patsubst %,../,$(subst /, ,$(@:%/GNUmakefile=%))))

SUB_MAKEFILES:=$(addsuffix GNUmakefile,$(sort $(dir $(SOURCES_FULL))))
createsubs: $(SUB_MAKEFILES)
$(SUB_MAKEFILES):
	@echo "Creating $@..."
	@echo "export OPENMSX_SUBSET=$(@:$(SOURCES_PATH)/%GNUmakefile=%)" > $@
	@echo "all:" >> $@
	@echo "	@\$$(MAKE) -C $(RELPATH) -f build/main.mk all" >> $@
# Force re-creation every time this target is run.
.PHONY: $(SUB_MAKEFILES)
endif

# Compile and generate dependency files in one go.
DEPEND_SUBST=$(patsubst $(SOURCES_PATH)/%.cc,$(DEPEND_PATH)/%.d,$<)
$(OBJECTS_FULL): $(INIT_DUMMY_FILE)
$(OBJECTS_FULL): $(OBJECTS_PATH)/%.o: $(SOURCES_PATH)/%.cc $(DEPEND_PATH)/%.d
	@echo "Compiling $(patsubst $(SOURCES_PATH)/%,%,$<)..."
	@mkdir -p $(@D)
	@mkdir -p $(patsubst $(OBJECTS_PATH)%,$(DEPEND_PATH)%,$(@D))
	@$(COMPILE_ENV) $(CXX) \
		$(DEPEND_FLAGS) -MMD -MF $(DEPEND_SUBST) \
		-o $@ $(CXXFLAGS) $(COMPILE_FLAGS) -c $<
	@touch $@ # Force .o file to be newer than .d file.
# Generate dependencies that do not exist yet.
# This is only in case some .d files have been deleted;
# in normal operation this rule is never triggered.
$(DEPEND_FULL):

# Windows resources that are added to the executable.
ifneq ($(filter mingw%,$(OPENMSX_TARGET_OS)),)
$(RESOURCE_HEADER): $(INIT_DUMMY_FILE) forceversionextraction
	@$(PYTHON) $(RESOURCE_SCRIPT) $@
$(RESOURCE_OBJ): $(RESOURCE_SRC) $(RESOURCE_HEADER)
	@echo "Compiling resources..."
	@mkdir -p $(@D)
	@$(WINDRES) $(addprefix --include-dir=,$(^D)) -o $@ -i $<
endif

# Link executable.
ifeq ($(OPENMSX_TARGET_CPU),univ)
BINARY_FOR_CPU=$(BINARY_FULL:derived/univ-%=derived/$(1)-%)
SINGLE_CPU_BINARIES=$(foreach CPU,$(CPU_LIST),$(call BINARY_FOR_CPU,$(CPU)))

.PHONY: $(SINGLE_CPU_BINARIES)
$(SINGLE_CPU_BINARIES):
	@echo "Start compile for $(firstword $(subst -, ,$(@:derived/%=%))) CPU..."
	@$(MAKE) -f build/main.mk all \
		OPENMSX_TARGET_CPU=$(firstword $(subst -, ,$(@:derived/%=%))) \
		OPENMSX_TARGET_OS=$(OPENMSX_TARGET_OS) \
		OPENMSX_FLAVOUR=$(OPENMSX_FLAVOUR) \
		3RDPARTY_FLAG=$(3RDPARTY_FLAG) \
		PYTHON=$(PYTHON)
	@echo "Finished compile for $(firstword $(subst -, ,$(@:derived/%=%))) CPU."

$(BINARY_FULL): $(SINGLE_CPU_BINARIES)
	@mkdir -p $(@D)
	@lipo -create $^ -output $@
else
$(BINARY_FULL): $(OBJECTS_FULL) $(RESOURCE_OBJ)
ifeq ($(OPENMSX_SUBSET),)
	@echo "Linking $(notdir $@)..."
	@mkdir -p $(@D)
	@+$(LINK_ENV) $(CXX) -o $@ $(CXXFLAGS) $^ $(LINK_FLAGS)
  ifeq ($(STRIP_SEPARATE),true)
	@echo "Stripping $(notdir $@)..."
	@strip $@
  endif
  ifeq ($(USE_SYMLINK),true)
	@ln -sf $(@:derived/%=%) derived/$(BINARY_FILE)
  else
	@cp $@ derived/$(BINARY_FILE)
  endif
else
	@echo "Not linking $(notdir $@) because only a subset was built."
endif # subset
endif # universal binary

$(LIBRARY_FULL): $(OBJECTS_FULL) $(RESOURCE_OBJ)
	@echo "Linking $(notdir $@)..."
	@mkdir -p $(@D)
	@$(LINK_ENV) $(CXX) -o $@ $(CXXFLAGS) $^ $(LINK_FLAGS)

# Run executable.
run: all
	@echo "Running $(notdir $(BINARY_FULL))..."
	@$(BINARY_FULL)


# Installation and Binary Packaging
# =================================

ifneq ($(filter $(MAKECMDGOALS),bindist)$(filter $(OPENMSX_TARGET_OS),darwin),)
# Create binary distribution directory.

BINDIST_DIR:=$(BUILD_PATH)/bindist
BINDIST_PACKAGE:=

# Override install locations.
INSTALL_ROOT:=$(BINDIST_DIR)/install
ifneq ($(filter mingw%,$(OPENMSX_TARGET_OS)),)
# In Windows the "share" dir is expected at the same level as the executable,
# so do not put the executable in "bin".
INSTALL_BINARY_DIR:=$(INSTALL_ROOT)
else
INSTALL_BINARY_DIR:=$(INSTALL_ROOT)/bin
endif
INSTALL_SHARE_DIR:=$(INSTALL_ROOT)/share
INSTALL_DOC_DIR:=$(INSTALL_ROOT)/doc
# C-BIOS should be included.
INSTALL_CONTRIB:=true
# Do not display header and post-install instructions.
INSTALL_VERBOSE:=false

.PHONY: bindist bindistclean

bindist: install

# Force removal of old destination dir before installing to new dir.
install: bindistclean

bindistclean: $(BINARY_FULL)
	@echo "Removing any old binary package..."
	@rm -rf $(BINDIST_DIR)
	@$(if $(BINDIST_PACKAGE),rm -f $(BINDIST_PACKAGE),)
	@echo "Creating binary package:"
endif
ifeq ($(OPENMSX_TARGET_OS),darwin)
# Application directory for Darwin.
# This handles the "bindist" target, but can also be used with the "install"
# target to create an app folder but no DMG.
include build/package-darwin/app.mk
else
ifeq ($(OPENMSX_TARGET_OS),dingux)
# ZIP file package for Dingux.
include build/package-dingux/zip.mk
else
# Note: Use OPENMSX_INSTALL only to create binary packages.
#       To change installation dir for actual installations, edit "custom.mk".
OPENMSX_INSTALL?=$(INSTALL_BASE)
# Allow full customization of locations, used by Debian packaging.
INSTALL_BINARY_DIR?=$(OPENMSX_INSTALL)/bin
INSTALL_SHARE_DIR?=$(OPENMSX_INSTALL)/share
INSTALL_DOC_DIR?=$(OPENMSX_INSTALL)/doc
INSTALL_VERBOSE?=true
endif
endif

# DESTDIR is a convention shared by at least GNU and FreeBSD to specify a path
# prefix that will be used for all installed files.
install: $(BINARY_FULL)
	@$(PYTHON) build/install.py "$(DESTDIR)" \
		$(INSTALL_BINARY_DIR) $(INSTALL_SHARE_DIR) $(INSTALL_DOC_DIR) \
		$(BINARY_FULL) $(OPENMSX_TARGET_OS) \
		$(INSTALL_VERBOSE) $(INSTALL_CONTRIB)


# Source Packaging
# ================

dist:
	@$(PYTHON) build/gitdist.py


# Binary Packaging Using 3rd Party Libraries
# ==========================================

.PHONY: $(addprefix 3rdparty-,$(CPU_LIST)) run-3rdparty

3rdparty: $(addprefix 3rdparty-,$(CPU_LIST))

# Recursive invocation for different CPUs.
# This is used even when building for a single CPU, since the OS and flavour
# can differ.
$(addprefix 3rdparty-,$(CPU_LIST)):
	$(MAKE) -f build/main.mk run-3rdparty \
		OPENMSX_TARGET_CPU=$(@:3rdparty-%=%) \
		OPENMSX_TARGET_OS=$(OPENMSX_TARGET_OS) \
		OPENMSX_FLAVOUR=$(OPENMSX_FLAVOUR) \
		3RDPARTY_FLAG=true \
		PYTHON=$(PYTHON)

# Call third party Makefile with the right arguments.
# This is an internal target, users should select "3rdparty" instead.
run-3rdparty:
	$(MAKE) -f build/3rdparty.mk \
		BUILD_PATH=$(BUILD_PATH)/3rdparty \
		OPENMSX_TARGET_CPU=$(OPENMSX_TARGET_CPU) \
		OPENMSX_TARGET_OS=$(OPENMSX_TARGET_OS) \
		_CC=$(CC) _CFLAGS="$(TARGET_FLAGS) $(CXXFLAGS)" \
		_LD=$(LD) _LDFLAGS="$(TARGET_FLAGS)" \
		LINK_MODE=$(LINK_MODE) $(COMPILE_ENV) \
		PYTHON=$(PYTHON)

staticbindist: 3rdparty
	$(MAKE) -f build/main.mk bindist \
		OPENMSX_TARGET_CPU=$(OPENMSX_TARGET_CPU) \
		OPENMSX_TARGET_OS=$(OPENMSX_TARGET_OS) \
		OPENMSX_FLAVOUR=$(OPENMSX_FLAVOUR) \
		3RDPARTY_FLAG=true \
		PYTHON=$(PYTHON)

endif # PLATFORM
