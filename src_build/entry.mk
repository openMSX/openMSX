# Entry point of build system.
#
# Do a sanity check on the given action(s) and processes them sequentially.
# Sequential processing is needed because for example "clean" and "all" cannot
# run in parallel. Some actions might be able to run in parallel, but that is
# an optimization we can do later, if it is really worth it.

# All actions we want to expose to the user.
USER_ACTIONS:=\
	3rdparty all app bindist clean createsubs dist install probe run \
	staticbindist

# Mark all actions as logical targets.
.PHONY: $(USER_ACTIONS)

# Reject unknown actions.
UNKNOWN_ACTIONS:=$(filter-out $(USER_ACTIONS),$(MAKECMDGOALS))
ifneq ($(UNKNOWN_ACTIONS),)
ifeq ($(words $(UNKNOWN_ACTIONS)),1)
$(error Unknown action: $(UNKNOWN_ACTIONS))
else
$(error Unknown actions: $(UNKNOWN_ACTIONS))
endif
endif

ifeq ($(MAKECMDGOALS),)
# Make default action explicit.
MAKECMDGOALS:=all
.PHONY: default
default: all
endif

ifeq ($(words $(MAKECMDGOALS)),1)
# Single action, run it in this Make process.
include src_build/main.mk
else
# Multiple actions are given, process them sequentially.
# If the same action is given more than once, some warnings will be displayed,
# but they will all be executed.
ACTION_COUNTER:=$(MAKECMDGOALS:%=x)
include src_build/entry-seq.mk
$(MAKECMDGOALS): sequence-$(words $(MAKECMDGOALS))
	@true
endif
