# $Id$
# Should be included at the start of each node.mk file.

# Get name of current directory.
SUBDIR:=$(firstword $(SUBDIRSTACK))
SUBDIRSTACK:=$(wordlist 2,$(words $(SUBDIRSTACK)),$(SUBDIRSTACK))
# Push current directory on directory stack.
DIRSTACK:=$(CURDIR) $(DIRSTACK)
CURDIR:=$(CURDIR)$(SUBDIR)

# Initialise node vars with empty value.
SUBDIRS:=
DIST:=
SRC_HDR:=
SRC_ONLY:=
HDR_ONLY:=
SRC_HDR_:=
SRC_ONLY_:=
HDR_ONLY_:=
SRC_HDR_true:=
SRC_ONLY_true:=
HDR_ONLY_true:=
SRC_HDR_false:=
SRC_ONLY_false:=
HDR_ONLY_false:=

