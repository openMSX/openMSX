# $Id$

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker \
	DirtyChecker \
	VDPSettings FrameSkipSetting \
	Icon \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLRenderer \
	SDLGLRenderer GLUtil \
	DummyRenderer \
	XRenderer \
	BitmapConverter CharacterConverter \
	Scalers

HDR_ONLY:= \
	DisplayMode.hh \
	VRAMObserver.hh \
	SpriteConverter.hh \
	Blender.hh \
	GLUtil.hh

$(eval $(PROCESS_NODE))

