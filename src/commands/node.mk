# $Id$

include build/node-start.mk

SRC_HDR:= \
	CommandException \
	GlobalCommandController \
	MSXCommandController \
	ProxyCommand \
	Completer \
	Command \
	InfoCommand \
	InfoTopic \
	Interpreter \
	TclObject \
	TclParser \

HDR_ONLY:= \
	InterpreterOutput \
	CommandController

include build/node-end.mk

