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

# Used-adjustable values:

# Generic compilation flags.
CXXFLAGS:=-pipe
# Stricter warning and error reporting.
CXXFLAGS+=-Wall
# Optimisation flags.
CXXFLAGS+=-O3 -mcpu=pentiumpro -march=pentiumpro -ffast-math -funroll-loops

LIBS_EXTERNAL:=stdc++ xml2 SDL SDL_image GL
INCLUDE_EXTERNAL:=/usr/include/libxml2

BUILD_PATH:=derived

# No user-servicable parts below:

# Logical targets which require dependency files.
DEPEND_TARGETS:=all
# Logical targets which do not require dependency files.
NODEPEND_TARGETS:=clean
# Mark all logical targets as such.
.PHONY: $(DEPEND_TARGETS) $(NODEPEND_TARGETS)

SOURCES_PATH:=src
# TODO: Use node.mk system for building sources list.
SOURCES_FULL:=$(sort $(shell find $(SOURCES_PATH) -name "*.cc"))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/debugger/Debugger.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/debugger/Views.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/thread/testCondVar.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/libxmlx/xmlxdump.cc,$(SOURCES_FULL))
SOURCES:=$(SOURCES_FULL:$(SOURCES_PATH)/%.cc=%)

DEPEND_PATH:=$(BUILD_PATH)/dep
DEPEND_FULL:=$(addsuffix .d,$(addprefix $(DEPEND_PATH)/,$(SOURCES)))

LIBS_FLAGS:=$(addprefix -l,$(LIBS_EXTERNAL))

INCLUDE_INTERNAL:=$(filter-out %/CVS,$(shell find $(SOURCES_PATH) -type d))
INCLUDE_FLAGS:=$(addprefix -I,$(INCLUDE_INTERNAL) $(INCLUDE_EXTERNAL))

OBJECTS_PATH:=$(BUILD_PATH)/obj
OBJECTS_FULL:=$(addsuffix .o,$(addprefix $(OBJECTS_PATH)/,$(SOURCES)))

BINARY_PATH:=$(BUILD_PATH)/bin
BINARY_FULL:=$(BINARY_PATH)/openmsx

# Default target; make sure this is always the first target in this Makefile.
all: $(BINARY_FULL)

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
	@echo "Linking $(patsubst $(BINARY_PATH)/%,%,$@)..."
	@mkdir -p $(@D)
	@gcc -o $@ $(LIBS_FLAGS) $^
