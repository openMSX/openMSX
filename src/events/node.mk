# $Id$

SRC_HDR:= \
	EventDistributor \
	HotKey \
	AfterCommand \
	Keys \
	KeyEventInserter SDLEventInserter \
	CliCommInput CliCommOutput 

HDR_ONLY:= \
	EventListener.hh

$(eval $(PROCESS_NODE))

