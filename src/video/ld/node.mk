# $Id$

include build/node-start.mk

SRC_HDR_$(COMPONENT_LASERDISC)+= LDSDLRasterizer LDDummyRasterizer \
			LDPixelRenderer LDRenderer LDDummyRenderer

HDR_ONLY:= 

include build/node-end.mk

