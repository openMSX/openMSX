# $Id$

SRC_HDR:= \
	EventDistributor \
	HotKey \
	Keys \
	KeyEventInserter SDLEventInserter \
	CliCommInput CliCommOutput 

HDR_ONLY:= \
	EventListener.hh

$(eval $(PROCESS_NODE))

