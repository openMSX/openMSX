# $Id$

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker \
	DirtyChecker \
	VDPSettings FrameSkipSetting \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLRenderer \
	SDLGLRenderer GLUtil \
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

