# $Id$
#
# Declares a dependency between each pair of consecutive actions in
# ACTION_SEQUENCE.

$(word 2,$(ACTION_SEQUENCE)): $(word 1,$(ACTION_SEQUENCE))
ACTION_SEQUENCE:=$(wordlist 2,999999,$(ACTION_SEQUENCE))
ifneq ($(words $(ACTION_SEQUENCE)),1)
include build/entry-seq.mk
endif
