# $Id$

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker \
	DirtyChecker \
	VDPSettings FrameSkipSetting \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLRenderer \
	SDLGLRenderer \
	XRenderer \
	BitmapConverter CharacterConverter

HDR_ONLY:= \
	DisplayMode.hh \
	VRAMObserver.hh \
	SpriteConverter.hh \
	Blender.hh \
	GLUtil.hh

$(eval $(PROCESS_NODE))

