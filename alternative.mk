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

# Default build flavour: probably the best for most users.
OPENMSX_FLAVOUR?=i686

BUILD_BASE:=derived
BUILD_PATH:=$(BUILD_BASE)/$(OPENMSX_FLAVOUR)

# Load flavour specific settings.
CXXFLAGS:=
LDFLAGS:=
include config-$(OPENMSX_FLAVOUR).mk

# Generic compilation flags.
CXXFLAGS+=-pipe
# Stricter warning and error reporting.
CXXFLAGS+=-Wall

# Flags for profiling.
OPENMSX_PROFILE?=false
ifneq ($(OPENMSX_PROFILE),true)
  ifneq ($(OPENMSX_PROFILE),false)
    $(error Value of OPENMSX_PROFILE ("$(OPENMSX_PROFILE)") should be "true" or "false")
  endif
endif
ifeq ($(OPENMSX_PROFILE),true)
  CXXFLAGS+=-pg
  BUILD_PATH:=$(BUILD_PATH)-profile
endif

# Strip binary?
OPENMSX_STRIP?=false
ifneq ($(OPENMSX_STRIP),true)
  ifneq ($(OPENMSX_STRIP),false)
    $(error Value of OPENMSX_STRIP ("$(OPENMSX_STRIP)") should be "true" or "false")
  endif
endif
ifeq ($(OPENMSX_PROFILE),true)
  OPENMSX_STRIP:=false
endif
ifeq ($(OPENMSX_STRIP),true)
  LDFLAGS+=--strip-all
endif

# Logical targets which require dependency files.
DEPEND_TARGETS:=all run
# Logical targets which do not require dependency files.
NODEPEND_TARGETS:=clean
# Mark all logical targets as such.
.PHONY: $(DEPEND_TARGETS) $(NODEPEND_TARGETS)

SOURCES_PATH:=src
# TODO: Use node.mk system for building sources list.
SOURCES_FULL:=$(sort $(shell find $(SOURCES_PATH)/$(OPENMSX_SUBSET) -name "*.cc"))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/debugger/Debugger.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/debugger/Views.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/thread/testCondVar.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/libxmlx/xmlxdump.cc,$(SOURCES_FULL))
SOURCES:=$(SOURCES_FULL:$(SOURCES_PATH)/%.cc=%)

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

# Determine include flags.
INCLUDE_INTERNAL:=$(filter-out %/CVS,$(shell find $(SOURCES_PATH) -type d))
INCLUDE_FLAGS:=$(addprefix -I,$(INCLUDE_INTERNAL))
INCLUDE_FLAGS+=$(foreach lib,$(LIBS_CONFIG),$(shell $(lib)-config --cflags))

# Determine link flags.
LINK_FLAGS_PREFIX:=-Wl,
LINK_FLAGS:=$(addprefix $(LINK_FLAGS_PREFIX),$(LDFLAGS))
LINK_FLAGS+=$(addprefix -l,$(LIBS_PLAIN))
LINK_FLAGS+=$(foreach lib,$(LIBS_CONFIG),$(shell $(lib)-config --libs))

# Default target; make sure this is always the first target in this Makefile.
all: config $(BINARY_FULL)
# GNU Make 3.80 supports "|" in dependency list to force an order:
# all: config | $(BINARY_FULL)

config:
	@echo "Build configuration:"
	@echo "  Flavour: $(OPENMSX_FLAVOUR)"
	@echo "  Profile: $(OPENMSX_PROFILE)"
	@echo "  Subset:  $(if $(OPENMSX_SUBSET),$(OPENMSX_SUBSET),full build)"

# Include dependency files.
ifeq ($(filter $(NODEPEND_TARGETS),$(MAKECMDGOALS)),)
  -include $(DEPEND_FULL)
endif

# Clean up entire build tree.
clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_PATH)

# Compile and generate dependency files in one go.
DEPEND_SUBST=$(patsubst $(SOURCES_PATH)/%.cc,$(DEPEND_PATH)/%.d,$<)
$(OBJECTS_FULL): $(OBJECTS_PATH)/%.o: $(SOURCES_PATH)/%.cc $(DEPEND_PATH)/%.d
	@echo "Compiling $(patsubst $(SOURCES_PATH)/%,%,$<)..."
	@mkdir -p $(@D)
	@mkdir -p $(patsubst $(OBJECTS_PATH)%,$(DEPEND_PATH)%,$(@D))
	@gcc \
		-MMD -MT $(DEPEND_SUBST) -MF $(DEPEND_SUBST) \
		-o $@ $(CXXFLAGS) $(INCLUDE_FLAGS) -c $<
	@touch $@ # Force .o file to be newer than .d file.
$(DEPEND_FULL):

# Link executable.
$(BINARY_FULL): $(OBJECTS_FULL)
ifeq ($(OPENMSX_SUBSET),)
	@echo "Linking $(notdir $@)..."
	@mkdir -p $(@D)
	@gcc -o $@ $(CXXFLAGS) $(LINK_FLAGS) $^
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
