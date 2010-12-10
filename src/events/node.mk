# $Id$

include build/node-start.mk

SRC_HDR:= \
	EventDistributor \
	HotKey \
	AfterCommand \
	Keys \
	CliComm GlobalCliComm MSXCliComm StdioMessages \
	CliServer CliConnection Socket \
	Event InputEvents \
	InputEventGenerator InputEventFactory

HDR_ONLY:= \
	CliMessages \
	EventListener \
	FinishFrameEvent

include build/node-end.mk

