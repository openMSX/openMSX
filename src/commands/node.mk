# $Id$

include build/node-start.mk

SRC_HDR:= \
	CommandException \
	CommandController \
	InfoCommand \
	TCLInterp

HDR_ONLY:= \
	Command \
	InfoTopic

include build/node-end.mk

