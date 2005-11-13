# $Id$

include build/node-start.mk

SRC_HDR:= \
	EventDistributor \
	HotKey \
	AfterCommand \
	Keys \
	CliComm CliServer CliConnection Socket \
	InputEventGenerator

HDR_ONLY:= \
	EventListener \
	Event \
	InputEvents \
	LedEvent \
	FinishFrameEvent

include build/node-end.mk

