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
DEPEND_TARGETS:=all run
# Logical targets which do not require dependency files.
NODEPEND_TARGETS:=clean config
# Mark all logical targets as such.
.PHONY: $(DEPEND_TARGETS) $(NODEPEND_TARGETS)


# Flavours
# ========

# Function to check a boolean variable has value "true" or "false".
# Usage: $(call BOOLCHECK,VARIABLE_NAME)
BOOLCHECK=$(if $(filter-out true false,$($(1))), \
	$(error Value of $(1) ("$($(1))") should be "true" or "false") )

# Default build flavour: probably the best for most users.
OPENMSX_FLAVOUR?=i686

BUILD_BASE:=derived
BUILD_PATH:=$(BUILD_BASE)/$(OPENMSX_FLAVOUR)

# Load flavour specific settings.
CXXFLAGS:=
LDFLAGS:=
include config-$(OPENMSX_FLAVOUR).mk

# Determine compiler.
OPENMSX_CXX?=g++
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


# Filesets
# ========

SOURCES_PATH:=src
# TODO: Use node.mk system for building sources list.
SOURCES_FULL:=$(sort $(shell find $(SOURCES_PATH)/$(OPENMSX_SUBSET) -name "*.cc"))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/debugger/Debugger.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/thread/testCondVar.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/libxmlx/xmlxdump.cc,$(SOURCES_FULL))
SOURCES:=$(SOURCES_FULL:$(SOURCES_PATH)/%.cc=%)

HEADERS_FULL:=$(sort $(shell find $(SOURCES_PATH)/$(OPENMSX_SUBSET) -name "*.hh"))
HEADERS:=$(HEADERS_FULL:$(SOURCES_PATH)/%=%)

DEPEND_PATH:=$(BUILD_PATH)/dep
DEPEND_FULL:=$(addsuffix .d,$(addprefix $(DEPEND_PATH)/,$(SOURCES)))

OBJECTS_PATH:=$(BUILD_PATH)/obj
OBJECTS_FULL:=$(addsuffix .o,$(addprefix $(OBJECTS_PATH)/,$(SOURCES)))

BINARY_PATH:=$(BUILD_PATH)/bin
BINARY_FULL:=$(BINARY_PATH)/openmsx

LOG_PATH:=$(BUILD_PATH)/log

# Libraries that do not have a lib-config script.
LIBS_PLAIN:=stdc++ SDL_image GL
# Libraries that have a lib-config script.
LIBS_CONFIG:=xml2 sdl


# Compile and Link Flags
# ======================

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
LINK_FLAGS:=$(addprefix $(LINK_FLAGS_PREFIX),$(LDFLAGS))
LINK_FLAGS+=$(addprefix -l,$(LIBS_PLAIN))
LINK_FLAGS+=$(foreach lib,$(LIBS_CONFIG),$(shell $(lib)-config --libs))
ifeq ($(OPENMSX_FLAVOUR),gcc34)
# TODO: Temp to force g++ 3.4-pre to pick up the right libs.
LINK_FLAGS:=-L/opt/gcc-cvs/lib $(LINK_FLAGS)
endif


# Build Rules
# ===========

# Default target; make sure this is always the first target in this Makefile.
MAKECMDGOALS?=all
all: config $(BINARY_FULL)

config:
	@echo "Build configuration:"
	@echo "  Flavour: $(OPENMSX_FLAVOUR)"
	@echo "  Profile: $(OPENMSX_PROFILE)"
	@echo "  Subset:  $(if $(OPENMSX_SUBSET),$(OPENMSX_SUBSET),full build)"

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
	@$(CXX) -o $@ $(CXXFLAGS) $(LINK_FLAGS) $^
	@ln -sf $(@:$(BUILD_BASE)/%=%) $(BUILD_BASE)/$(notdir $@)
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
