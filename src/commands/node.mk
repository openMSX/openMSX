# $Id$

include build/node-start.mk

SRC_HDR:= \
	CommandException \
	CommandController \
	InfoCommand \
	TCLInterp \
	Interpreter

HDR_ONLY:= \
	Command \
	InfoTopic

include build/node-end.mk

