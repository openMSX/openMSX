# $Id$

SRC_HDR:= \
	CommandException \
	CommandController \
	InfoCommand \
	AliasCommands \
	TCLInterp

HDR_ONLY:= \
	Command \
	InfoTopic

$(eval $(PROCESS_NODE))

