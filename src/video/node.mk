# $Id$

include build/node-start.mk

SUBDIRS:= \
	v9990

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker \
	DirtyChecker \
	VDPSettings \
	Icon \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLRenderer \
	DummyRenderer \
	XRenderer \
	BitmapConverter CharacterConverter \
	Scaler SimpleScaler SaI2xScaler Scale2xScaler HQ2xScaler \
	Deinterlacer \
	ScreenShotSaver

HDR_ONLY:= \
	DisplayMode \
	VRAMObserver \
	SpriteConverter \
	Blender \
	GLUtil

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLRenderer GLUtil

include build/node-end.mk

