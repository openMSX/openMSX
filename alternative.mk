# $Id$
#
# Alternative Makefile
# ====================
#
# An experiment to see if auto* is worth the effort.
#
# Used a lot of ideas from Peter Miller's excellent paper
# "Recursive Make Considered Harmful".
# http://www.tip.net.au/~millerp/rmch/recu-make-cons-harm.html


# Logical Targets
# ===============

# Logical targets which require dependency files.
DEPEND_TARGETS:=all run default
# Logical targets which do not require dependency files.
NODEPEND_TARGETS:=clean config
# Mark all logical targets as such.
.PHONY: $(DEPEND_TARGETS) $(NODEPEND_TARGETS)

# Default target; make sure this is always the first target in this Makefile.
MAKECMDGOALS?=default
default: all


# Build Base
# ==========

# All created files will be inside this directory.
BUILD_BASE:=derived


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
BOOLCHECK=$(strip \
	$(call DEFCHECK,$(1)) \
	$(if $(filter-out _true _false,_$($(1))), \
		$(error Value of $(1) ("$($(1))") should be "true" or "false") ) \
	)

# Shell function for indenting output.
# Usage: command | $(INDENT)
INDENT:=sed -e "s/^/  /"

# Will be added to by platform specific Makefile, by flavour specific Makefile
# and by this Makefile.
# Note: LDFLAGS are passed to the linker itself, LINK_FLAGS are passed to the
#       compiler in the link phase.
CXXFLAGS:=
LDFLAGS:=
LINK_FLAGS:=


# Platforms
# =========

# Note:
# A platform currently specifies both the host platform (performing the build)
# and the target platform (running the created binary). When we have real
# experience with cross-compilation, a more sophisticated system can be
# designed.

DETECTSYS_PATH:=$(BUILD_BASE)/detectsys
DETECTSYS_MAKE:=$(DETECTSYS_PATH)/detectsys.mk
DETECTSYS_SCRIPT:=$(DETECTSYS_PATH)/detectsys
DETECTSYS_AC:=detectsys.ac
DETECTSYS_INPUT:=detectsys.mk.in

-include $(DETECTSYS_MAKE)

$(DETECTSYS_MAKE): $(DETECTSYS_SCRIPT) $(DETECTSYS_INPUT)
	@echo "Autodetecting native system:"
	@cp $(DETECTSYS_INPUT) $(@D)
	@cd $(@D) ; sh $(notdir $(DETECTSYS_SCRIPT)) | $(INDENT)

# Note: This step needs autoconf, but by shipping DETECTSYS_SCRIPT in source
#       releases, we avoid this step being triggered.
$(DETECTSYS_SCRIPT): $(DETECTSYS_AC)
	@echo "Creating system autodetect script..."
	@mkdir -p $(@D)
	@cp $? $(@D)
	@cd $(@D) ; autoconf $< > $(@F)


ifneq ($(origin OPENMSX_PLATFORM),undefined)

# Load platform specific settings.
$(call DEFCHECK,OPENMSX_PLATFORM)
include platform-$(OPENMSX_PLATFORM).mk

# Check that all expected variables were defined by platform specific Makefile:
# - executable file name extension
$(call DEFCHECK,EXEEXT)
# - library names
$(call DEFCHECK,LIBS_PLAIN)
$(call DEFCHECK,LIBS_CONFIG)
# - flavour (user selectable; platform specific default)
$(call DEFCHECK,OPENMSX_FLAVOUR)
# - platform supports symlinks?
$(call BOOLCHECK,USE_SYMLINK)


# Flavours
# ========

# Load flavour specific settings.
include flavour-$(OPENMSX_FLAVOUR).mk


# Filesets
# ========

define PROCESS_SUBDIR
# Push current directory on directory stack.
DIRSTACK:=$$(CURDIR) $$(DIRSTACK)
CURDIR:=$(CURDIR)/$(1)
# Initialise node vars with empty value.
SUBDIRS:=
SRC_HDR:=
SRC_ONLY:=
HDR_ONLY:=
# Include node Makefile.
include $$(CURDIR)/node.mk
# Pop current directory off directory stack.
CURDIR:=$$(firstword $$(DIRSTACK))
DIRSTACK:=$$(wordlist 2,$$(words $$(DIRSTACK)),$$(DIRSTACK))
endef

define PROCESS_NODE
# Process this node.
SOURCES_FULL+=$$(sort \
	$$(addprefix $$(CURDIR)/,$$(addsuffix .cc,$(SRC_HDR))) \
	$$(addprefix $$(CURDIR)/,$$(SRC_ONLY)) \
	)
HEADERS_FULL+=$$(sort \
	$$(addprefix $$(CURDIR)/,$$(addsuffix .hh,$(SRC_HDR))) \
	$$(addprefix $$(CURDIR)/,$$(HDR_ONLY)) \
	)
# Process subnodes.
$$(foreach dir,$$(sort $$(SUBDIRS)),$$(eval $$(call PROCESS_SUBDIR,$$(dir))))
endef

BUILD_PATH:=$(BUILD_BASE)/$(OPENMSX_PLATFORM)-$(OPENMSX_FLAVOUR)

SOURCES_PATH:=src

# Force evaluation upon assignment.
SOURCES_FULL:=
HEADERS_FULL:=
# Include root node.
CURDIR:=$(SOURCES_PATH)
include $(CURDIR)/node.mk
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

BINARY_PATH:=$(BUILD_PATH)/bin
BINARY_FULL:=$(BINARY_PATH)/openmsx$(EXEEXT)

LOG_PATH:=$(BUILD_PATH)/log


# Compiler and Flags
# ==================

# Determine compiler.
$(call DEFCHECK,OPENMSX_CXX)
CXX:=$(OPENMSX_CXX)

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

# Flags for profiling.
OPENMSX_PROFILE?=false
$(call BOOLCHECK,OPENMSX_PROFILE)
ifeq ($(OPENMSX_PROFILE),true)
  CXXFLAGS+=-pg
  BUILD_PATH:=$(BUILD_PATH)-profile
endif

# Strip binary?
OPENMSX_STRIP?=false
$(call BOOLCHECK,OPENMSX_STRIP)
ifeq ($(OPENMSX_PROFILE),true)
  # Profiling does not work with stripped binaries, so override.
  OPENMSX_STRIP:=false
endif
ifeq ($(OPENMSX_STRIP),true)
  LDFLAGS+=--strip-all
endif

# Generic compilation flags.
CXXFLAGS+=-pipe
# Stricter warning and error reporting.
CXXFLAGS+=-Wall

# Determine include flags.
INCLUDE_INTERNAL:=$(filter-out %/CVS,$(shell find $(SOURCES_PATH) -type d))
INCLUDE_FLAGS:=$(addprefix -I,$(INCLUDE_INTERNAL))
INCLUDE_FLAGS+=$(foreach lib,$(LIBS_CONFIG),$(shell $(lib)-config --cflags))

# Determine link flags.
LINK_FLAGS_PREFIX:=-Wl,
LINK_FLAGS+=$(addprefix $(LINK_FLAGS_PREFIX),$(LDFLAGS))
LINK_FLAGS+=$(addprefix -l,$(LIBS_PLAIN))
LINK_FLAGS+=$(foreach lib,$(LIBS_CONFIG),$(shell $(lib)-config --libs))


# Build Rules
# ===========

all: config $(BINARY_FULL)

config:
	@echo "Build configuration:"
	@echo "  Platform: $(OPENMSX_PLATFORM)"
	@echo "  Flavour:  $(OPENMSX_FLAVOUR)"
	@echo "  Profile:  $(OPENMSX_PROFILE)"
	@echo "  Subset:   $(if $(OPENMSX_SUBSET),$(OPENMSX_SUBSET),full build)"

# Include dependency files.
ifneq ($(filter $(DEPEND_TARGETS),$(MAKECMDGOALS)),)
  -include $(DEPEND_FULL)
endif

# Clean up build tree of current flavour.
clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_PATH)

# Compile and generate dependency files in one go.
DEPEND_SUBST=$(patsubst $(SOURCES_PATH)/%.cc,$(DEPEND_PATH)/%.d,$<)
$(OBJECTS_FULL): $(OBJECTS_PATH)/%.o: $(SOURCES_PATH)/%.cc $(DEPEND_PATH)/%.d
	@echo "Compiling $(patsubst $(SOURCES_PATH)/%,%,$<)..."
	@mkdir -p $(@D)
	@mkdir -p $(patsubst $(OBJECTS_PATH)%,$(DEPEND_PATH)%,$(@D))
	@$(CXX) $(PRECOMPH_FLAGS) \
		-MMD -MP -MT $(DEPEND_SUBST) -MF $(DEPEND_SUBST) \
		-o $@ $(CXXFLAGS) $(INCLUDE_FLAGS) -c $<
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
	@ln -sf $(@:$(BUILD_BASE)/%=%) $(BUILD_BASE)/$(notdir $@)
  else
	@cp $@ $(BUILD_BASE)
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
	@$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) $<

else
# USE_PRECOMPH == false
PRECOMPH_FLAGS:=
endif

endif # OPENMSX_PLATFORM
