# $Id$

include build/node-start.mk

SRC_HDR:= \
	CommandException \
	CommandController \
	Command \
	InfoCommand \
	Interpreter \
	CommandArgument

HDR_ONLY:= \
	InfoTopic

include build/node-end.mk

