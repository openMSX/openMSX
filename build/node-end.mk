# $Id$
# Should be included at the end of each node.mk file.

# Process this node.
SOURCES_FULL+=$(sort \
	$(addprefix $(CURDIR),$(addsuffix .cc, \
	$(SRC_HDR) $(SRC_HDR_true) $(SRC_ONLY) $(SRC_ONLY_true) \
	)))
DIST_FULL+=$(sort \
	$(addprefix $(CURDIR),$(addsuffix .cc, \
	$(SRC_HDR_) $(SRC_HDR_false) $(SRC_ONLY_) $(SRC_ONLY_false) \
	)))
HEADERS_FULL+=$(sort \
	$(addprefix $(CURDIR),$(addsuffix .hh, \
	$(SRC_HDR) $(SRC_HDR_true) $(HDR_ONLY) $(HDR_ONLY_true) \
	)))
DIST_FULL+=$(sort \
	$(addprefix $(CURDIR),$(addsuffix .hh, \
	$(SRC_HDR_) $(SRC_HDR_false) $(HDR_ONLY_) $(HDR_ONLY_false) \
	)))
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
