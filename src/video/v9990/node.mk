# $Id$

include build/node-start.mk

SRC_HDR:= \
	V9990 V9990VRAM \
	V9990CmdEngine \
	V9990DisplayTiming \
	V9990Renderer \
	V9990DummyRenderer \
	V9990PixelRenderer \
	V9990SDLRasterizer \
	V9990DummyRasterizer \
	V9990BitmapConverter

HDR_ONLY:= \
	V9990Rasterizer \
	V9990Renderer \
	V9990ModeEnum

SRC_HDR_$(COMPONENT_GL)+= \
	V9990GLRasterizer

include build/node-end.mk

