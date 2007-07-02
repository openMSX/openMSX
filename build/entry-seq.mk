# $Id$
#
# Declares a dependency between each pair of consecutive goals in GOAL_SEQUENCE.

$(word 2,$(GOAL_SEQUENCE)): $(word 1,$(GOAL_SEQUENCE))
GOAL_SEQUENCE:=$(wordlist 2,999999,$(GOAL_SEQUENCE))
ifneq ($(words $(GOAL_SEQUENCE)),1)
include build/entry-seq.mk
endif
