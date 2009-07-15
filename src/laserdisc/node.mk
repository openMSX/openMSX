# $Id$

include build/node-start.mk

SRC_HDR_$(COMPONENT_LASERDISC)+= OggReader LaserdiscPlayerCLI \
				LaserdiscPlayer PioneerLDControl yuv2rgb

HDR_ONLY:= 

include build/node-end.mk

