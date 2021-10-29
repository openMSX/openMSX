# Declares a dependency chain of sequence-<N> on sequence<N-1>, sequence-<N-1>
# on sequence<N-2> etc, where N is the number of words in ACTION_COUNTER.

.PHONY: sequence-$(words $(ACTION_COUNTER))
ifeq ($(words $(ACTION_COUNTER)),0)
sequence-0:
else
NEXT_ACTION_COUNTER:=$(wordlist 2,999999,$(ACTION_COUNTER))

sequence-$(words $(ACTION_COUNTER)): sequence-$(words $(NEXT_ACTION_COUNTER))
	@echo Action: $(word $(@:sequence-%=%),$(MAKECMDGOALS))
	@$(MAKE) --no-print-directory -f src_build/main.mk \
		$(word $(@:sequence-%=%),$(MAKECMDGOALS))

ACTION_COUNTER:=$(NEXT_ACTION_COUNTER)
include src_build/entry-seq.mk
endif
