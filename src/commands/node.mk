# $Id$

include build/node-start.mk

SRC_HDR:= \
	CommandException \
	GlobalCommandController \
	MSXCommandController \
	Completer \
	Command \
	InfoCommand \
	InfoTopic \
	Interpreter \
	TclObject \

HDR_ONLY:= \
	InterpreterOutput \
	CommandController

include build/node-end.mk

