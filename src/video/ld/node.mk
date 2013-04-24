include build/node-start.mk

SRC_HDR_$(COMPONENT_LASERDISC)+= \
	LDSDLRasterizer \
	LDPixelRenderer LDRenderer LDDummyRenderer

HDR_ONLY:= LDRasterizer

include build/node-end.mk
