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

.PHONY: all

# Used-adjustable values:

CXXFLAGS=-O3 -mcpu=pentiumpro -march=pentiumpro -ffast-math -funroll-loops

LIBS_EXTERNAL:=stdc++ gcc_s xml2 SDL SDL_image GL
INCLUDE_EXTERNAL:=/usr/include/libxml2

BUILD_PATH:=derived

# No user-servicable parts below:

SOURCES_PATH:=src
# TODO: Use node.mk system for building sources list.
SOURCES_FULL:=$(sort $(shell find $(SOURCES_PATH) -name "*.cc"))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/debugger/Debugger.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/debugger/Views.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/thread/testCondVar.cc,$(SOURCES_FULL))
SOURCES_FULL:=$(filter-out $(SOURCES_PATH)/libxmlx/xmlxdump.cc,$(SOURCES_FULL))
SOURCES:=$(SOURCES_FULL:$(SOURCES_PATH)/%.cc=%)

LIBS_FLAGS:=$(addprefix -l,$(LIBS_EXTERNAL))

INCLUDE_INTERNAL:=$(filter-out %/CVS,$(shell find $(SOURCES_PATH) -type d))
INCLUDE_FLAGS:=$(addprefix -I,$(INCLUDE_INTERNAL) $(INCLUDE_EXTERNAL))

OBJECTS_PATH:=$(BUILD_PATH)/obj
OBJECTS_FULL:=$(addsuffix .o,$(addprefix $(OBJECTS_PATH)/,$(SOURCES)))

BINARY_PATH:=$(BUILD_PATH)/bin
BINARY_FULL:=$(BINARY_PATH)/openmsx

# TODO: Generate dependency files.

all: $(BINARY_FULL)

clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_PATH)

$(OBJECTS_PATH)/%.o: $(SOURCES_PATH)/%.cc
	@echo "Compiling $(patsubst $(SOURCES_PATH)/%,%,$<)..."
	@mkdir -p $(@D)
	@gcc -o $@ $(CXXFLAGS) $(INCLUDE_FLAGS) -c $<

$(BINARY_FULL): $(OBJECTS_FULL)
	@echo "Linking $(patsubst $(BINARY_PATH)/%,%,$@)..."
	@mkdir -p $(@D)
	@gcc -o $@ $(LIBS_FLAGS) $^
