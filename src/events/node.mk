# $Id$

SRC_HDR:= \
	EventDistributor \
	HotKey \
	Keys \
	KeyEventInserter SDLEventInserter \
	Command CommandController

HDR_ONLY:= \
	EventListener.hh

$(eval $(PROCESS_NODE))

