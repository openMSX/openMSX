# $Id$
# Should be included at the end of each node.mk file.

# Backwards compatibility for auto* system:
DIST+=$(if $(filter ./$(SOURCES_PATH)%,$(CURDIR)),Makefile.am,)

# Process this node.
SOURCES_FULL+=$(sort \
	$(addprefix $(CURDIR),$(addsuffix .cc,$(SRC_HDR) $(SRC_ONLY))) \
	)
HEADERS_FULL+=$(sort \
	$(addprefix $(CURDIR),$(addsuffix .hh,$(SRC_HDR) $(HDR_ONLY))) \
	)
DIST_FULL+=$(sort \
	$(addprefix $(CURDIR),$(DIST) node.mk) \
	)

# Process subnodes.
ifneq ($(SUBDIRS),)
SUBDIRS:=$(addsuffix /,$(SUBDIRS))
SUBDIRSTACK:=$(SUBDIRS) $(SUBDIRSTACK)
include $(addprefix $(CURDIR),$(addsuffix node.mk,$(SUBDIRS)))
endif

# Pop current directory off directory stack.
CURDIR:=$(firstword $(DIRSTACK))
DIRSTACK:=$(wordlist 2,$(words $(DIRSTACK)),$(DIRSTACK))
