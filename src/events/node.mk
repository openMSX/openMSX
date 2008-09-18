# $Id$

include build/node-start.mk

SRC_HDR:= \
	EventDistributor \
	HotKey \
	AfterCommand \
	Keys \
	CliComm GlobalCliComm MSXCliComm \
	CliServer CliConnection Socket \
	Event InputEvents \
	InputEventGenerator InputEventFactory

HDR_ONLY:= \
	EventListener \
	FinishFrameEvent

include build/node-end.mk

