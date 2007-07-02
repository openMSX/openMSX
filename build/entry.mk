# $Id$
#
# Entry point of build system.
#
# Do a sanity check on the given goal(s) and processes them sequentially.
# Sequential processing is needed because for example "clean" and "all" cannot
# run in parallel. Some goals might be able to run in parallel, but that is an
# optimization we can do later, if it is really worth it.

# All goals we want to expose to the user.
USER_GOALS:=3rdparty all bindist clean dist install probe run staticbindist

# Mark all goals as logical targets.
.PHONY: $(USER_GOALS)

# Reject unknown goals.
UNKNOWN_GOALS:=$(filter-out $(USER_GOALS),$(MAKECMDGOALS))
ifneq ($(UNKNOWN_GOALS),)
ifeq ($(words $(UNKNOWN_GOALS)),1)
$(error Unknown goal: $(UNKNOWN_GOALS))
else
$(error Unknown goals: $(UNKNOWN_GOALS))
endif
endif

# Make default goal explicit.
MAKECMDGOALS?=all

ifeq ($(words $(MAKECMDGOALS)),1)
# Single goal, run it in actual Makefile.
include build/main.mk
else
# Multiple goals are given, process them sequentially.
GOAL_SEQUENCE:=$(MAKECMDGOALS)
include build/entry-seq.mk
$(MAKECMDGOALS):
	@$(MAKE) --no-print-directory -f build/main.mk $@
endif
