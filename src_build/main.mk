# openMSX Build System
# ====================
#
# This is the home made build system for openMSX, which replaced the
# autoconf/automake combo.
#
# Used a lot of ideas from Peter Miller's excellent paper
# "Recursive Make Considered Harmful".
# http://miller.emu.id.au/pmiller/books/rmch/


# Verbosity
# =========

# V=0: Summary only
# V=1: Command only
# V=2: Summary + command
V?=0
SUM:=@echo
ifeq ($V,0)
CMD:=@
else
CMD:=
ifeq ($V,1)
SUM:=@\#
else ifneq ($V,2)
$(warning Unsupported value for verbosity flag "V": $V)
endif
endif

# Python Interpreter
# ==================

PYTHON?=python3
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
NODEPEND_TARGETS:=clean config probe 3rdparty run-3rdparty staticbindist
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
COMPILE_FLAGS:=-pthread
# Note: LDFLAGS are passed to the linker itself, LINK_FLAGS are passed to the
#       compiler in the link phase.
LDFLAGS:=
LINK_FLAGS:=-pthread
# Flags that specify the target platform.
# These should be inherited by the 3rd party libs Makefile.
TARGET_FLAGS:=


# Customisation
# =============

include src_build/custom.mk
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
DETECTSYS_SCRIPT:=src_build/detectsys.py
LOCAL_PLATFORM:=$(shell $(PYTHON) $(DETECTSYS_SCRIPT))
ifeq ($(LOCAL_PLATFORM),)
$(error No platform specified using OPENMSX_TARGET_CPU and OPENMSX_TARGET_OS and autodetection of local platform failed)
endif
OPENMSX_TARGET_CPU:=$(word 1,$(LOCAL_PLATFORM))
OPENMSX_TARGET_OS:=$(word 2,$(LOCAL_PLATFORM))
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
include src_build/platform-$(OPENMSX_TARGET_OS).mk
# Check that all expected variables were defined by OS specific Makefile:
# - library file name extension
$(call DEFCHECK,LIBRARYEXT)
# - executable file name extension
$(call DEFCHECK,EXEEXT)
# - platform supports symlinks?
$(call BOOLCHECK,USE_SYMLINK)

# Get CPU specific flags.
TARGET_FLAGS+=$(shell $(PYTHON) src_build/cpu2flags.py $(OPENMSX_TARGET_CPU))


# Flavours
# ========

# Load flavour specific settings.
include src_build/flavour-$(OPENMSX_FLAVOUR).mk

UNITTEST?=false


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
    "import version; print(version.extractRevisionString())" \
    )
  BINARY_FULL:=$(BINARY_PATH)/openmsx-$(REVISION)$(EXEEXT)
else
  BINARY_FULL:=$(BINARY_PATH)/$(BINARY_FILE)
endif

LIBRARY_FILE:=openmsx$(LIBRARYEXT)
LIBRARY_PATH:=$(BUILD_PATH)/lib
LIBRARY_FULL:=$(LIBRARY_PATH)/$(LIBRARY_FILE)

ifeq ($(OPENMSX_TARGET_OS),android)
MAIN_EXECUTABLE:=$(LIBRARY_FULL)
else
MAIN_EXECUTABLE:=$(BINARY_FULL)
endif

BUILDINFO_SCRIPT:=src_build/buildinfo2code.py
CONFIG_HEADER:=$(BUILD_PATH)/config/build-info.hh
PROBE_SCRIPT:=src_build/probe.py
PROBE_MAKE:=$(BUILD_PATH)/config/probed_defs.mk
VERSION_SCRIPT:=src_build/version2code.py
VERSION_HEADER:=$(BUILD_PATH)/config/Version.ii
COMPONENTS_HEADER_SCRIPT:=src_build/components2code.py
COMPONENTS_DEFS_SCRIPT:=src_build/components2defs.py
COMPONENTS_HEADER:=$(BUILD_PATH)/config/components.hh
COMPONENTS_DEFS:=$(BUILD_PATH)/config/components_defs.mk
GENERATED_HEADERS:=$(VERSION_HEADER) $(CONFIG_HEADER) $(COMPONENTS_HEADER)


# Configuration
# =============

ifneq ($(filter $(DEPEND_TARGETS),$(MAKECMDGOALS)),)
-include $(PROBE_MAKE)
-include $(COMPONENTS_DEFS)
endif # goal requires dependencies


# Filesets
# ========

SOURCE_DIRS:=$(sort $(shell find src -type d))

SOURCES_FULL:=$(foreach dir,$(SOURCE_DIRS),$(sort $(wildcard $(dir)/*.cc)))
ifeq ($(OPENMSX_TARGET_OS),darwin)
SOURCES_FULL+=$(foreach dir,$(SOURCE_DIRS),$(sort $(wildcard $(dir)/*.mm)))
endif
SOURCES_FULL:=$(filter-out %Test.cc,$(SOURCES_FULL))

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

ifneq ($(COMPONENT_ALSAMIDI),true)
SOURCES_FULL:=$(filter-out src/serial/MidiSessionALSA.cc,$(SOURCES_FULL))
endif

ifeq ($(UNITTEST),true)
SOURCES_FULL:=$(filter-out src/main.cc,$(SOURCES_FULL))
else
SOURCES_FULL:=$(filter-out src/unittest/%.cc,$(SOURCES_FULL))
endif

# Apply subset to sources list.
SOURCES_FULL:=$(filter $(SOURCES_PATH)/$(OPENMSX_SUBSET)%,$(SOURCES_FULL))
ifeq ($(SOURCES_FULL),)
$(error Sources list empty $(if \
   $(OPENMSX_SUBSET),after applying subset "$(OPENMSX_SUBSET)*"))
endif

SOURCES:=$(SOURCES_FULL:$(SOURCES_PATH)/%=%)

DEPEND_PATH:=$(BUILD_PATH)/dep
DEPEND_FULL:=$(addsuffix .d,$(addprefix $(DEPEND_PATH)/,$(SOURCES)))

OBJECTS_PATH:=$(BUILD_PATH)/obj
OBJECTS_FULL:=$(addsuffix .o,$(addprefix $(OBJECTS_PATH)/,$(SOURCES)))

ifneq ($(filter mingw%,$(OPENMSX_TARGET_OS)),)
RESOURCE_SRC:=src/resource/openmsx.rc
RESOURCE_OBJ:=$(OBJECTS_PATH)/resources.o
RESOURCE_SCRIPT:=src_build/win_resource.py
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
  # Enable C++17 (for the most part supported since clang-5)
  COMPILE_FLAGS+=-std=c++17 -fconstexpr-steps=2000000
  COMPILE_FLAGS+=-Wall -Wextra -Wundef -Wno-invalid-offsetof -Wunused-macros -Wdouble-promotion -Wmissing-declarations -Wshadow -Wold-style-cast -Wzero-as-null-pointer-constant
  # Hardware descriptions can contain constants that are not used in the code
  # but still useful as documentation.
  COMPILE_FLAGS+=-Wno-unused-const-variable
  CC:=$(subst clang++,clang,$(CXX))
  DEPEND_FLAGS+=-MP
else
ifneq ($(filter %g++,$(CXX))$(filter g++%,$(CXX))$(findstring /g++-,$(CXX)),)
  # Generic compilation flags.
  COMPILE_FLAGS+=-pipe
  # Enable C++17  (almost fully supported since gcc-7)
  COMPILE_FLAGS+=-std=c++17
  # Stricter warning and error reporting.
  COMPILE_FLAGS+=-Wall -Wextra -Wundef -Wno-invalid-offsetof -Wunused-macros -Wdouble-promotion -Wmissing-declarations -Wshadow -Wold-style-cast -Wzero-as-null-pointer-constant
  # Empty definition of used headers, so header removal doesn't break things.
  DEPEND_FLAGS+=-MP
  # Plain C compiler, for the 3rd party libs.
  CC:=$(subst g++,gcc,$(CXX))
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

# Add compile and link flags for libraries (from COMPONENTS_DEFS).
COMPILE_FLAGS+=$(LIBRARY_COMPILE_FLAGS)
LINK_FLAGS+=$(LIBRARY_LINK_FLAGS)


# Build Rules
# ===========

# Force a probe if "probe" target is passed explicitly.
ifeq ($(MAKECMDGOALS),probe)
probe: $(PROBE_MAKE)
.PHONY: $(PROBE_MAKE)
endif

# Probe for headers and functions.
$(PROBE_MAKE): $(PROBE_SCRIPT) src_build/custom.mk \
		src_build/systemfuncs2code.py src_build/systemfuncs.py
	$(CMD)$(PYTHON) $(PROBE_SCRIPT) \
		"$(CXX) $(TARGET_FLAGS)" \
		$(@D) $(OPENMSX_TARGET_OS) $(LINK_MODE) "$(3RDPARTY_INSTALL_DIR)"
	$(CMD)touch $@

# Generate configuration header.
# TODO: One platform file may include another, so the real solution would be
#       for the Python script to write dependency info.
$(CONFIG_HEADER): $(BUILDINFO_SCRIPT) \
		src_build/custom.mk src_build/platform-$(OPENMSX_TARGET_OS).mk
	$(CMD)$(PYTHON) $(BUILDINFO_SCRIPT) $@ \
		$(OPENMSX_TARGET_OS) $(OPENMSX_TARGET_CPU) $(OPENMSX_FLAVOUR) \
		$(INSTALL_SHARE_DIR)
	$(CMD)touch $@

# Generate version header.
.PHONY: forceversionextraction
forceversionextraction:
$(VERSION_HEADER): forceversionextraction
	$(CMD)$(PYTHON) $(VERSION_SCRIPT) $@

# Generate components header.
$(COMPONENTS_HEADER): $(COMPONENTS_HEADER_SCRIPT) $(PROBE_MAKE) \
		src_build/components.py
	$(CMD)$(PYTHON) $(COMPONENTS_HEADER_SCRIPT) $@ $(PROBE_MAKE)
	$(CMD)touch $@

# Generate components Makefile.
$(COMPONENTS_DEFS): $(COMPONENTS_DEFS_SCRIPT) $(PROBE_MAKE) \
		src_build/components.py
	$(CMD)$(PYTHON) $(COMPONENTS_DEFS_SCRIPT) $@ $(PROBE_MAKE)
	$(CMD)touch $@

# Default target.
ifeq ($(OPENMSX_TARGET_OS),darwin)
all: app
else
all: $(MAIN_EXECUTABLE)
endif

ifeq ($(COMPONENT_CORE),false)
# Force new probe.
config: $(PROBE_MAKE)
	$(CMD)mv $(PROBE_MAKE) $(PROBE_MAKE).failed
	@false
else
# Print configuration.
config:
	@echo "Build configuration:"
	@echo "  Platform: $(PLATFORM)"
	@echo "  Flavour:  $(OPENMSX_FLAVOUR)"
	@echo "  Compiler: $(CXX)"
	@echo "  Subset:   $(if $(OPENMSX_SUBSET),$(OPENMSX_SUBSET)*,full build)"
endif

# Include dependency files.
ifneq ($(filter $(DEPEND_TARGETS),$(MAKECMDGOALS)),)
  -include $(DEPEND_FULL)
endif

# Clean up build tree of current flavour.
clean:
	$(SUM) "Cleaning up..."
	$(CMD)rm -rf $(BUILD_PATH)

# Create Makefiles in source subdirectories, to conveniently build a subset.
ifeq ($(MAKECMDGOALS),createsubs)
# Function that concatenates list items to form a single string.
# Usage: $(call JOIN,TEXT)
JOIN=$(if $(1),$(firstword $(1))$(call JOIN,$(wordlist 2,999999,$(1))),)

RELPATH=$(call JOIN,$(patsubst %,../,$(subst /, ,$(@:%/GNUmakefile=%))))

SUB_MAKEFILES:=$(addsuffix GNUmakefile,$(sort $(dir $(SOURCES_FULL))))
createsubs: $(SUB_MAKEFILES)
$(SUB_MAKEFILES):
	$(SUM) "Creating $@..."
	$(CMD)echo "export OPENMSX_SUBSET=$(@:$(SOURCES_PATH)/%GNUmakefile=%)" > $@
	$(CMD)echo "all:" >> $@
	$(CMD)echo "	@\$$(MAKE) -C $(RELPATH) -f src_build/main.mk all" >> $@
# Force re-creation every time this target is run.
.PHONY: $(SUB_MAKEFILES)
endif

# Compile and generate dependency files in one go.
DEPEND_SUBST=$(patsubst $(SOURCES_PATH)/%,$(DEPEND_PATH)/%.d,$<)
$(OBJECTS_FULL): $(OBJECTS_PATH)/%.o: $(SOURCES_PATH)/% $(DEPEND_PATH)/%.d \
		| config $(GENERATED_HEADERS)
	$(SUM) "Compiling $(patsubst $(SOURCES_PATH)/%,%,$<)..."
	$(CMD)mkdir -p $(@D)
	$(CMD)mkdir -p $(patsubst $(OBJECTS_PATH)%,$(DEPEND_PATH)%,$(@D))
	$(CMD)$(CXX) \
		$(DEPEND_FLAGS) -MMD -MF $(DEPEND_SUBST) \
		-o $@ $(CXXFLAGS) $(COMPILE_FLAGS) -c $<
	$(CMD)touch $@ # Force .o file to be newer than .d file.
# Generate dependencies that do not exist yet.
# This is only in case some .d files have been deleted;
# in normal operation this rule is never triggered.
$(DEPEND_FULL):

# Windows resources that are added to the executable.
ifneq ($(filter mingw%,$(OPENMSX_TARGET_OS)),)
$(RESOURCE_HEADER): forceversionextraction | config
	$(CMD)$(PYTHON) $(RESOURCE_SCRIPT) $@
$(RESOURCE_OBJ): $(RESOURCE_SRC) $(RESOURCE_HEADER)
	$(SUM) "Compiling resources..."
	$(CMD)mkdir -p $(@D)
	$(CMD)$(WINDRES) $(addprefix --include-dir=,$(^D)) -o $@ -i $<
endif

# Link executable.
$(BINARY_FULL): $(OBJECTS_FULL) $(RESOURCE_OBJ)
ifeq ($(OPENMSX_SUBSET),)
	$(SUM) "Linking $(notdir $@)..."
	$(CMD)mkdir -p $(@D)
	$(CMD)+$(CXX) -o $@ $(CXXFLAGS) $^ $(LINK_FLAGS)
  ifeq ($(STRIP_SEPARATE),true)
	$(SUM) "Stripping $(notdir $@)..."
	$(CMD)strip $@
  endif
  ifeq ($(USE_SYMLINK),true)
	$(CMD)ln -sf $(@:derived/%=%) derived/$(BINARY_FILE)
  else
	$(CMD)cp $@ derived/$(BINARY_FILE)
  endif
else
	$(SUM) "Not linking $(notdir $@) because only a subset was built."
endif # subset

$(LIBRARY_FULL): $(OBJECTS_FULL) $(RESOURCE_OBJ)
	$(SUM) "Linking $(notdir $@)..."
	$(CMD)mkdir -p $(@D)
	$(CMD)$(CXX) -shared -o $@ $(CXXFLAGS) $^ $(LINK_FLAGS)

# Run executable.
run: all
	$(SUM) "Running $(notdir $(BINARY_FULL))..."
	$(CMD)$(BINARY_FULL)


# Installation and Binary Packaging
# =================================

ifneq ($(filter $(MAKECMDGOALS),bindist)$(filter $(OPENMSX_TARGET_OS),darwin),)
# Create binary distribution directory.

BINDIST_DIR:=$(BUILD_PATH)/bindist
BINDIST_PACKAGE:=

# Override install locations.
DESTDIR:=$(BINDIST_DIR)/install
INSTALL_ROOT:=
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

bindistclean: $(MAIN_EXECUTABLE)
	$(SUM) "Removing any old binary package..."
	$(CMD)rm -rf $(BINDIST_DIR)
	$(CMD)$(if $(BINDIST_PACKAGE),rm -f $(BINDIST_PACKAGE),)
	$(SUM) "Creating binary package:"
endif
ifeq ($(OPENMSX_TARGET_OS),darwin)
# Application directory for Darwin.
# This handles the "bindist" target, but can also be used with the "install"
# target to create an app folder but no DMG.
include src_build/package-darwin/app.mk
else
ifeq ($(OPENMSX_TARGET_OS),dingux)
# ZIP file package for Dingux.
include src_build/package-dingux/opk.mk
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
install: $(MAIN_EXECUTABLE)
	$(CMD)$(PYTHON) src_build/install.py "$(DESTDIR)" \
		"$(INSTALL_BINARY_DIR)" "$(INSTALL_SHARE_DIR)" "$(INSTALL_DOC_DIR)" \
		$(MAIN_EXECUTABLE) $(OPENMSX_TARGET_OS) \
		$(INSTALL_VERBOSE) $(INSTALL_CONTRIB)


# Source Packaging
# ================

dist:
	$(CMD)$(PYTHON) src_build/gitdist.py


# Binary Packaging Using 3rd Party Libraries
# ==========================================

# Recursive invocation with 3RDPARTY_FLAG=true.
3rdparty:
	$(MAKE) -f src_build/main.mk run-3rdparty \
		OPENMSX_TARGET_CPU=$(OPENMSX_TARGET_CPU) \
		OPENMSX_TARGET_OS=$(OPENMSX_TARGET_OS) \
		OPENMSX_FLAVOUR=$(OPENMSX_FLAVOUR) \
		3RDPARTY_FLAG=true \
		PYTHON=$(PYTHON)

# Call third party Makefile with the right arguments.
# This is an internal target, users should select "3rdparty" instead.
run-3rdparty:
	$(MAKE) -f src_build/3rdparty.mk \
		BUILD_PATH=$(BUILD_PATH)/3rdparty \
		OPENMSX_TARGET_CPU=$(OPENMSX_TARGET_CPU) \
		OPENMSX_TARGET_OS=$(OPENMSX_TARGET_OS) \
		_CC=$(CC) _CFLAGS="$(TARGET_FLAGS) $(CXXFLAGS)" \
		_LDFLAGS="$(TARGET_FLAGS)" \
		WINDRES=$(WINDRES) \
		LINK_MODE=$(LINK_MODE) \
		PYTHON=$(PYTHON)

staticbindist: 3rdparty
	$(MAKE) -f src_build/main.mk bindist \
		OPENMSX_TARGET_CPU=$(OPENMSX_TARGET_CPU) \
		OPENMSX_TARGET_OS=$(OPENMSX_TARGET_OS) \
		OPENMSX_FLAVOUR=$(OPENMSX_FLAVOUR) \
		3RDPARTY_FLAG=true \
		PYTHON=$(PYTHON)

endif # PLATFORM
