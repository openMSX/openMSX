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
SRC_HDR:=
SRC_ONLY:=
HDR_ONLY:=
DIST:=

