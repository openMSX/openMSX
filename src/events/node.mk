# $Id$

SRC_HDR:= \
	EventDistributor \
	HotKey \
	AfterCommand \
	Keys \
	KeyEventInserter SDLEventInserter \
	CliCommInput CliCommOutput 

HDR_ONLY:= \
	EventListener

$(eval $(PROCESS_NODE))

