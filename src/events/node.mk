# $Id$

SRC_HDR:= \
	EventDistributor \
	HotKey \
	Keys \
	KeyEventInserter SDLEventInserter \
	Command CommandController \
	CliCommunicator

HDR_ONLY:= \
	EventListener.hh

$(eval $(PROCESS_NODE))

