# $Id$

include build/node-start.mk

SUBDIRS:= \
	v9990

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker \
	VDPSettings \
	Icon \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLVideoSystem SDLRenderer SDLSnow \
	SDLUtil \
	DummyRenderer \
	XRenderer \
	BitmapConverter CharacterConverter \
	Scaler SimpleScaler SaI2xScaler Scale2xScaler HQ2xScaler \
	Deinterlacer \
	ScreenShotSaver \
	Display VideoSystem \
	DummyVideoSystem

HDR_ONLY:= \
	DirtyChecker \
	DisplayMode \
	VRAMObserver \
	SpriteConverter \
	Blender

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLVideoSystem SDLGLRenderer GLSnow GLUtil

include build/node-end.mk

