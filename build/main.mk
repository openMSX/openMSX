# $Id$
#
# openMSX Build System
# ====================
#
# This is the home made build system for openMSX, which replaced the
# autoconf/automake combo.
#
# Used a lot of ideas from Peter Miller's excellent paper
# "Recursive Make Considered Harmful".
# http://www.tip.net.au/~millerp/rmch/recu-make-cons-harm.html

# TODO:
# - Move calculation of CFLAGS and LDFLAGS to components.mk?
# - Change output format of tcl-search.sh to match probed_defs.mk format.
# - Make XRenderer into a component.


# Logical Targets
# ===============

# Logical targets which require dependency files.
DEPEND_TARGETS:=all default install run app
# Logical targets which do not require dependency files.
NODEPEND_TARGETS:=clean config probe
# Mark all logical targets as such.
.PHONY: $(DEPEND_TARGETS) $(NODEPEND_TARGETS)

# Default target; make sure this is always the first target in this Makefile.
MAKECMDGOALS?=default
default: all


# Base Directories
# ================

# All created files will be inside this directory.
BUILD_BASE:=derived

# All global Makefiles are inside this directory.
MAKE_PATH:=build


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

# Function to check whether a variable equals a given string.
# If it does, the string "true" is returned, otherwise "false".
# Usage: $(call EQUALS,VARIABLE_NAME,STRING)
EQUALS=$(DEFCHECK)$(if $(filter $(2),$($(1))),true,false)

# Shell function for indenting output.
# Usage: command | $(INDENT)
# TODO: Disabled for now, the exit status of a piped command is the exit
#       status of the command after the pipe, which is not what we want
#       in the case of indenting.
INDENT:=sed -e "s/^/  /"

# Will be added to by platform specific Makefile, by flavour specific Makefile
# and by this Makefile.
# Note: LDFLAGS are passed to the linker itself, LINK_FLAGS are passed to the
#       compiler in the link phase.
CXXFLAGS:=
LDFLAGS:=
LINK_FLAGS:=


# Customisation
# =============

include $(MAKE_PATH)/custom.mk
$(call DEFCHECK,INSTALL_BASE)
$(call BOOLCHECK,VERSION_EXEC)
$(call BOOLCHECK,SYMLINK_FOR_BINARY)
$(call BOOLCHECK,INSTALL_CONTRIB)


# Version
# =======

include $(MAKE_PATH)/version.mk
PACKAGE_FULL:=$(PACKAGE_NAME)-$(PACKAGE_VERSION)
CHANGELOG_REVISION:=\
	$(shell sed -ne "s/\$$Id: ChangeLog,v \([^ ]*\).*/\1/p" ChangeLog)


# Platforms
# =========

# Note:
# A platform currently specifies both the host platform (performing the build)
# and the target platform (running the created binary). When we have real
# experience with cross-compilation, a more sophisticated system can be
# designed.

ifeq ($(origin OPENMSX_TARGET_CPU),environment)
ifeq ($(origin OPENMSX_TARGET_OS),environment)
# Do not perform autodetection if platform was specified by the user.
else # OPENMSX_TARGET_OS not from environment
$(error You have specified OPENMSX_TARGET_CPU but not OPENMSX_TARGET_OS)
endif # OPENMSX_TARGET_OS
else # OPENMSX_TARGET_CPU not from environment
ifeq ($(origin OPENMSX_TARGET_OS),environment)
$(error You have specified OPENMSX_TARGET_OS but not OPENMSX_TARGET_CPU)
else # OPENMSX_TARGET_OS not from environment

DETECTSYS_PATH:=$(BUILD_BASE)/detectsys
DETECTSYS_MAKE:=$(DETECTSYS_PATH)/detectsys.mk
DETECTSYS_SCRIPT:=$(MAKE_PATH)/detectsys.sh

-include $(DETECTSYS_MAKE)

$(DETECTSYS_MAKE): $(DETECTSYS_SCRIPT)
	@echo "Autodetecting native system:"
	@mkdir -p $(@D)
	@sh $< > $@

endif # OPENMSX_TARGET_OS
endif # OPENMSX_TARGET_CPU

PLATFORM:=
ifneq ($(origin OPENMSX_TARGET_OS),undefined)
ifneq ($(origin OPENMSX_TARGET_CPU),undefined)
PLATFORM:=$(OPENMSX_TARGET_CPU)-$(OPENMSX_TARGET_OS)
endif
endif

# Ignore rest of Makefile if autodetection was not performed yet.
# Note that the include above will force a reload of the Makefile.
ifneq ($(PLATFORM),)

# Load CPU specific settings.
$(call DEFCHECK,OPENMSX_TARGET_CPU)
include $(MAKE_PATH)/cpu-$(OPENMSX_TARGET_CPU).mk
# Check that all expected variables were defined by OS specific Makefile:
# - endianess
$(call BOOLCHECK,BIG_ENDIAN)
# - flavour (user selectable; platform specific default)
$(call DEFCHECK,OPENMSX_FLAVOUR)

# Load OS specific settings.
$(call DEFCHECK,OPENMSX_TARGET_OS)
include $(MAKE_PATH)/platform-$(OPENMSX_TARGET_OS).mk
# Check that all expected variables were defined by OS specific Makefile:
# - executable file name extension
$(call DEFCHECK,EXEEXT)
# - platform supports symlinks?
$(call BOOLCHECK,USE_SYMLINK)


# Flavours
# ========

# Load flavour specific settings.
include $(MAKE_PATH)/flavour-$(OPENMSX_FLAVOUR).mk


# Profiling
# =========

OPENMSX_PROFILE?=false
$(call BOOLCHECK,OPENMSX_PROFILE)
ifeq ($(OPENMSX_PROFILE),true)
  CXXFLAGS+=-pg
endif


# Paths
# =====

BUILD_PATH:=$(BUILD_BASE)/$(PLATFORM)-$(OPENMSX_FLAVOUR)
ifeq ($(OPENMSX_PROFILE),true)
  BUILD_PATH:=$(BUILD_PATH)-profile
endif

SOURCES_PATH:=src

BINARY_PATH:=$(BUILD_PATH)/bin
BINARY_FILE:=openmsx$(EXEEXT)
ifeq ($(VERSION_EXEC),true)
  BINARY_FULL:=$(BINARY_PATH)/openmsx-dev$(CHANGELOG_REVISION)$(EXEEXT)
else
  BINARY_FULL:=$(BINARY_PATH)/$(BINARY_FILE)
endif

LOG_PATH:=$(BUILD_PATH)/log

CONFIG_PATH:=$(BUILD_PATH)/config
CONFIG_HEADER:=$(CONFIG_PATH)/build-info.hh
PROBE_SCRIPT:=$(MAKE_PATH)/probe.mk
PROBE_HEADER:=$(CONFIG_PATH)/probed_defs.hh
PROBE_MAKE:=$(CONFIG_PATH)/probed_defs.mk
VERSION_HEADER:=$(CONFIG_PATH)/Version.ii
COMPONENTS_MAKE:=$(MAKE_PATH)/components.mk
COMPONENTS_HEADER:=$(CONFIG_PATH)/components.hh
APP_MAKE:=$(MAKE_PATH)/package-darwin/app.mk


# Configuration
# =============

include $(MAKE_PATH)/info2code.mk
ifneq ($(filter $(DEPEND_TARGETS),$(MAKECMDGOALS)),)
-include $(PROBE_MAKE)
ifeq ($(PROBE_MAKE_INCLUDED),true)
include $(COMPONENTS_MAKE)
$(call BOOLCHECK,COMPONENT_CORE)
$(call BOOLCHECK,COMPONENT_GL)
$(call BOOLCHECK,COMPONENT_JACK)
endif
endif


# Filesets
# ========

# Force evaluation upon assignment.
SOURCES_FULL:=
HEADERS_FULL:=
DIST_FULL:=
# Include root node.
CURDIR:=
include node.mk
# Remove "./" in front of file names.
# It can cause trouble because Make removes it automatically in rules.
SOURCES_FULL:=$(SOURCES_FULL:./%=%)
HEADERS_FULL:=$(HEADERS_FULL:./%=%)
DIST_FULL:=$(DIST_FULL:./%=%)
# Apply subset to sources list.
SOURCES_FULL:=$(filter $(SOURCES_PATH)/$(OPENMSX_SUBSET)%,$(SOURCES_FULL))
ifeq ($(SOURCES_FULL),)
$(error Sources list empty $(if \
	$(OPENMSX_SUBSET),after applying subset "$(OPENMSX_SUBSET)*"))
endif
# Sanity check: only .cc files are allowed in sources list,
# because we don't have any way to build other sources.
NON_CC_SOURCES:=$(filter-out %.cc,$(SOURCES_FULL))
ifneq ($(NON_CC_SOURCES),)
$(error The following sources files do not have a .cc extension: \
$(NON_CC_SOURCES))
endif

SOURCES:=$(SOURCES_FULL:$(SOURCES_PATH)/%.cc=%)
HEADERS:=$(HEADERS_FULL:$(SOURCES_PATH)/%=%)

DEPEND_PATH:=$(BUILD_PATH)/dep
DEPEND_FULL:=$(addsuffix .d,$(addprefix $(DEPEND_PATH)/,$(SOURCES)))

OBJECTS_PATH:=$(BUILD_PATH)/obj
OBJECTS_FULL:=$(addsuffix .o,$(addprefix $(OBJECTS_PATH)/,$(SOURCES)))


# Compiler and Flags
# ==================

# Determine compiler.
$(call DEFCHECK,OPENMSX_CXX)
DEPEND_FLAGS:=
ifneq ($(filter %g++,$(OPENMSX_CXX))$(filter g++%,$(OPENMSX_CXX)),)
  # Generic compilation flags.
  CXXFLAGS+=-pipe
  # Stricter warning and error reporting.
  CXXFLAGS+=-Wall
  # Empty definition of used headers, so header removal doesn't break things.
  DEPEND_FLAGS+=-MP
else
  ifneq ($(filter %gcc,$(OPENMSX_CXX))$(filter gcc%,$(OPENMSX_CXX)),)
    $(error Set OPENMSX_CXX to your "g++" executable instead of "gcc")
  endif
  ifneq ($(filter %icc,$(OPENMSX_CXX)),)
    # Report all errors, warnings and remarks, except the following remarks:
    # (on the openmsx-devel list these were discussed and it was decided to
    # disable them since fixing them would not improve code quality)
    #  177: "handler parameter "e" was declared but never referenced"
    #  185: "dynamic initialization in unreachable code"
    #  271: "trailing comma is nonstandard"
    #  279: "controlling expression is constant"
    #  383: "value copied to temporary, reference to temporary used"
    #  869: "parameter [name] was never referenced"
    #  981: "operands are evaluated in unspecified order"
    CXXFLAGS+=-Wall -wd177,185,271,279,383,869,981
    # Temporarily disabled remarks: (may be re-enabled some time)
    #  111: "statement is unreachable"
    #       Occurs in template where code is unreachable for some expansions
    #       but not for others.
    #  444: "destructor for base class [class] is not virtual"
    #       Sometimes issued incorrectly.
    #       Reported to Intel: issue number 221909.
    #  530: "inline function [name] cannot be explicitly instantiated"
    #       Issued when explicitly instantiating template classes with inline
    #       methods. Needs more investigation / discussion.
    #  810: "conversion from [larger type] to [smaller type] may lose
    #       significant bits"
    #       Many instances not fixed yet, but should be fixed eventually.
    # 1125: "function [f1] is hidden by [f2] -- virtual function override
    #       intended?"
    #       Occurs lots of times on Command class. I don't understand why it
    #       thinks hiding is occurring there.
    # 1469: ""cc" clobber ignored"
    #       Seems to be caused by glibc headers.
    CXXFLAGS+=-wd111,444,530,810,1125,1469
  else
    $(warning Unsupported compiler: $(OPENMSX_CXX), please update Makefile)
  endif
endif
export CXX:=$(OPENMSX_CXX)
# Check if ccache usage was requested
ifeq ($(USE_CCACHE),true)
	CXX:=ccache $(CXX)
endif
# Use precompiled headers?
# TODO: Autodetect this: USE_PRECOMPH == compiler_is_g++ && g++_version >= 3.4
#       In new approach, Make >= 3.80 is needed for precompiled header rules.
USE_PRECOMPH?=false
$(call BOOLCHECK,USE_PRECOMPH)
# Problem: When using precompiled headers, the generated dependency files
#          only contain dependencies 1 level deep.
#          If generated without compiling as well, it depends on every
#          include there is.
# So one-shot compilation works great, but incremental compilation does not.

# Strip binary?
OPENMSX_STRIP?=false
$(call BOOLCHECK,OPENMSX_STRIP)
ifeq ($(OPENMSX_PROFILE),true)
  # Profiling does not work with stripped binaries, so override.
  OPENMSX_STRIP:=false
endif
ifeq ($(OPENMSX_STRIP),true)
  LINK_FLAGS+=-s
endif


# Determine common compile flags.
INCLUDE_INTERNAL:=$(filter-out %/CVS,$(shell find $(SOURCES_PATH) -type d))
INCLUDE_INTERNAL+=$(CONFIG_PATH)
INCLUDE_EXTERNAL:= # TODO: Define these here or platform-*.mk?
INCLUDE_EXTERNAL+=/usr/X11R6/include
COMPILE_FLAGS:=$(addprefix -I,$(INCLUDE_INTERNAL) $(INCLUDE_EXTERNAL))

# Determine common link flags.
LINK_FLAGS_PREFIX:=-Wl,
LINK_FLAGS+=$(addprefix $(LINK_FLAGS_PREFIX),$(LDFLAGS))

# Determine component specific compile and link flags.
ifeq ($(COMPONENT_CORE),true)
COMPILE_FLAGS+=$(foreach lib,$(CORE_LIBS),$($(lib)_CFLAGS))
LINK_FLAGS+=$(foreach lib,$(CORE_LIBS),$($(lib)_LDFLAGS))
endif
ifeq ($(COMPONENT_GL),true)
COMPILE_FLAGS+=$(GL_CFLAGS)
LINK_FLAGS+=$(GL_LDFLAGS)
endif
ifeq ($(COMPONENT_JACK),true)
COMPILE_FLAGS+=$(JACK_CFLAGS)
LINK_FLAGS+=$(JACK_LDFLAGS)
endif


# Build Rules
# ===========

# Do not build if core component dependencies are not met.
ifeq ($(COMPONENT_CORE),false)
$(error Cannot build openMSX because essential libraries are unavailable. \
Please install the needed libraries and their header files and rerun "configure")
endif

# Force a probe if "probe" target is passed explicitly.
ifneq ($(filter probe,$(MAKECMDGOALS)),)
probe: $(PROBE_MAKE)
.PHONY: $(PROBE_MAKE)
endif

# Probe for headers and functions.
# TODO: It would be cleaner to include probe.mk and probe-results.mk,
#       instead of executing them in a sub-make.
$(PROBE_MAKE): $(PROBE_SCRIPT) $(MAKE_PATH)/tcl-search.sh
	@OUTDIR=$(@D) OPENMSX_TARGET_OS=$(OPENMSX_TARGET_OS) COMPILE="$(CXX)" \
		$(MAKE) --no-print-directory -f $<
	@PROBE_MAKE=$(PROBE_MAKE) MAKE_PATH=$(MAKE_PATH) \
		$(MAKE) --no-print-directory -f $(MAKE_PATH)/probe-results.mk

all: $(VERSION_HEADER) $(CONFIG_HEADER) $(COMPONENTS_HEADER) \
	config $(BINARY_FULL)

# Print configuration.
config:
	@echo "Build configuration:"
	@echo "  Platform: $(PLATFORM)"
	@echo "  Flavour:  $(OPENMSX_FLAVOUR)"
	@echo "  Profile:  $(OPENMSX_PROFILE)"
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
ifneq ($(filter createsubs,$(MAKECMDGOALS)),)
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
	@echo "	@\$$(MAKE) -C $(RELPATH) -f build/main.mk" >> $@
# Force re-creation every time this target is run.
.PHONY: $(SUB_MAKEFILES)
endif

# Build application directory for Darwin.
ifeq ($(OPENMSX_TARGET_OS),darwin)
app: $(BINARY_FULL)
	@echo "Packaging application:"
	@OUTDIR=$(BUILD_PATH) BINARY=$< $(MAKE) --no-print-directory -f $(APP_MAKE)
endif

# Compile and generate dependency files in one go.
DEPEND_SUBST=$(patsubst $(SOURCES_PATH)/%.cc,$(DEPEND_PATH)/%.d,$<)
$(OBJECTS_FULL): $(OBJECTS_PATH)/%.o: $(SOURCES_PATH)/%.cc $(DEPEND_PATH)/%.d
	@echo "Compiling $(patsubst $(SOURCES_PATH)/%,%,$<)..."
	@mkdir -p $(@D)
	@mkdir -p $(patsubst $(OBJECTS_PATH)%,$(DEPEND_PATH)%,$(@D))
	@$(CXX) $(PRECOMPH_FLAGS) \
		$(DEPEND_FLAGS) -MMD -MF $(DEPEND_SUBST) \
		-o $@ $(CXXFLAGS) $(COMPILE_FLAGS) -c $<
	@touch $@ # Force .o file to be newer than .d file.
# Generate dependencies that do not exist yet.
# This is only in case some .d files have been deleted;
# in normal operation this rule is never triggered.
$(DEPEND_FULL):

# Link executable.
$(BINARY_FULL): $(OBJECTS_FULL)
ifeq ($(OPENMSX_SUBSET),)
	@echo "Linking $(notdir $@)..."
	@mkdir -p $(@D)
	@$(CXX) -o $@ $(CXXFLAGS) $^ $(LINK_FLAGS)
  ifeq ($(USE_SYMLINK),true)
	@ln -sf $(@:$(BUILD_BASE)/%=%) $(BUILD_BASE)/$(BINARY_FILE)
  else
	@cp $@ $(BUILD_BASE)/$(BINARY_FILE)
  endif
else
	@echo "Not linking $(notdir $@) because only a subset was built."
endif

# Run executable.
run: all
ifeq ($(OPENMSX_PROFILE),true)
	@echo "Profiling $(notdir $(BINARY_FULL))..."
	@cd $(dir $(BINARY_FULL)) ; ./$(notdir $(BINARY_FULL))
	@mkdir -p $(LOG_PATH)
	@echo "Creating report..."
	@gprof $(BINARY_FULL) $(dir $(BINARY_FULL))gmon.out \
		> $(LOG_PATH)/profile.txt
	@echo "Report written: $(LOG_PATH)/profile.txt"
else
	@echo "Running $(notdir $(BINARY_FULL))..."
	@$(BINARY_FULL)
endif


# Installation and Binary Packaging
# =================================

# Note: Use OPENMSX_INSTALL only to create binary packages.
#       To change installation dir for actual installations, edit "custom.mk".
OPENMSX_INSTALL?=$(INSTALL_BASE)
# Allow full customization of locations, used by Debian packaging.
INSTALL_BINARY_DIR?=$(OPENMSX_INSTALL)/bin
INSTALL_SHARE_DIR?=$(OPENMSX_INSTALL)/share
INSTALL_DOC_DIR?=$(OPENMSX_INSTALL)/doc

install: all
	@echo "Installing to $(OPENMSX_INSTALL):"
	@echo "  Executable..."
	@install -d $(INSTALL_BINARY_DIR)
	@install $(BINARY_FULL) $(INSTALL_BINARY_DIR)/$(BINARY_FILE)
	@echo "  Data files..."
	@sh build/install-recursive.sh share $(INSTALL_SHARE_DIR)
	@echo "  Documentation..."
	@install -d  $(INSTALL_DOC_DIR)
	@install -m 0644 README GPL AUTHORS $(INSTALL_DOC_DIR)
	@install -m 0644 $(addprefix doc/,$(INSTALL_DOCS)) $(INSTALL_DOC_DIR)
	@install -d $(INSTALL_DOC_DIR)/manual
	@install -m 0644 $(addprefix doc/manual/,*.html *.css *.png) \
		$(INSTALL_DOC_DIR)/manual
ifeq ($(INSTALL_CONTRIB),true)
	@echo "  C-BIOS..."
	@install -m 0644 Contrib/README.cbios $(INSTALL_DOC_DIR)/cbios.txt
	@sh build/install-recursive.sh Contrib/cbios $(INSTALL_SHARE_DIR)/machines
endif
ifeq ($(USE_SYMLINK),true)
	@echo "  Creating symlinks..."
	@ln -nsf National_CF-1200 $(INSTALL_SHARE_DIR)/machines/msx1
	@ln -nsf Philips_NMS_8250 $(INSTALL_SHARE_DIR)/machines/msx2
	@ln -nsf Panasonic_FS-A1FX $(INSTALL_SHARE_DIR)/machines/msx2plus
	@ln -nsf Panasonic_FS-A1GT $(INSTALL_SHARE_DIR)/machines/turbor
  ifeq ($(SYMLINK_FOR_BINARY),true)
	@if [ $(INSTALL_BINARY_DIR) != /usr/local/bin ]; then \
		if [ -d /usr/local/bin -a -w /usr/local/bin ]; then \
			ln -sf $(INSTALL_BINARY_DIR)/$(BINARY_FILE) /usr/local/bin/openmsx; \
		elif [ $(INSTALL_BINARY_DIR) != ~/bin -a -d ~/bin ]; then \
			ln -sf $(INSTALL_BINARY_DIR)/$(BINARY_FILE) ~/bin/openmsx; \
		fi; \
	fi
  endif
endif
	@echo "Installation complete... have fun!"
	@echo "Notice: if you want to emulate real MSX systems and not only the free C-BIOS machines, put the system ROMs in one of the following directories: $(INSTALL_SHARE_DIR)/systemroms or ~/.openMSX/share/systemroms"


# Source Packaging
# ================

DIST_BASE:=$(BUILD_BASE)/dist
DIST_PATH:=$(DIST_BASE)/$(PACKAGE_FULL)

dist: $(DETECTSYS_SCRIPT)
	@echo "Removing any old distribution files..."
	@rm -rf $(DIST_PATH)
	@echo "Gathering files for distribution..."
	@mkdir -p $(DIST_PATH)
	@cp -pr --parents $(DIST_FULL) $(DIST_PATH)
	@cp -p --parents $(HEADERS_FULL) $(DIST_PATH)
	@cp -p --parents $(SOURCES_FULL) $(DIST_PATH)
	@echo "Creating tarball..."
	@cd $(DIST_BASE) && \
		GZIP=--best tar zcf $(PACKAGE_FULL).tar.gz $(PACKAGE_FULL)


# Precompiled Headers
# ===================

ifeq ($(USE_PRECOMPH),true)

# Precompiled headers.
PRECOMPH_PATH:=$(BUILD_PATH)/hdr
PRECOMPH_COMB:=$(PRECOMPH_PATH)/all.h
PRECOMPH_FILE:=$(PRECOMPH_COMB).gch
PRECOMPH_FLAGS:=-include $(PRECOMPH_COMB)

$(OBJECTS_FULL): | $(PRECOMPH_FILE)

$(PRECOMPH_COMB): $(HEADERS_FULL)
	@echo "Generating combined header..."
	@mkdir -p $(PRECOMPH_PATH)
	@for header in $(HEADERS); do echo "#include \"$$header\""; done > $@

.DELETE_ON_ERROR: $(PRECOMPH_FILE)
$(PRECOMPH_FILE): $(PRECOMPH_COMB)
	@echo "Precompiling headers..."
	@$(CXX) $(CXXFLAGS) $(COMPILE_FLAGS) $<

else # USE_PRECOMPH == false
PRECOMPH_FLAGS:=
endif

endif # PLATFORM
