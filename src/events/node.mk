# $Id$

include build/node-start.mk

SRC_HDR:= \
	EventDistributor \
	HotKey \
	AfterCommand \
	Keys \
	SDLEventInserter \
	CliCommInput CliCommOutput \
	InputEventGenerator

HDR_ONLY:= \
	EventListener \
	Event \
	InputEvents \
	LedEvent \

include build/node-end.mk

