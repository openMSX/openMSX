# $Id$

include build/node-start.mk

SRC_HDR:= \
	EventDistributor \
	HotKey \
	AfterCommand \
	Keys \
	CliComm CliServer CliConnection Socket \
	Event InputEvents \
	InputEventGenerator InputEventFactory

HDR_ONLY:= \
	EventListener \
	LedEvent \
	FinishFrameEvent

include build/node-end.mk

