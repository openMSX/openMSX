# $Id$

SRC_HDR:= \
	CommandController \
	InfoCommand \
	AliasCommands

HDR_ONLY:= \
	Command \
	CommandException \
	InfoTopic

$(eval $(PROCESS_NODE))

