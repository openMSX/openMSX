# $Id$

include build/node-start.mk

SRC_HDR:= \
	CommandException \
	CommandController \
	Command \
	InfoCommand \
	TCLInterp \
	Interpreter \
	CommandArgument

HDR_ONLY:= \
	InfoTopic

include build/node-end.mk

