# $Id$

include build/node-start.mk

SRC_HDR:= \
	CommandException \
	CommandController \
	Command \
	InfoCommand \
	TCLInterp \
	Interpreter \
	TCLCommandResult

HDR_ONLY:= \
	InfoTopic \
	CommandResult

include build/node-end.mk

