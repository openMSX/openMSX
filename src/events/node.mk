# $Id$

SRC_HDR:= \
	EventDistributor \
	HotKey \
	AfterCommand \
	Keys \
	KeyEventInserter SDLEventInserter \
	CliCommInput CliCommOutput \
	InputEventGenerator

HDR_ONLY:= \
	EventListener \
	Event \
	InputEvents

$(eval $(PROCESS_NODE))

