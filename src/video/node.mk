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
	Scaler SimpleScaler SaI2xScaler Scale2xScaler HQ2xScaler \
	Deinterlacer \
	ScreenShotSaver

HDR_ONLY:= \
	DisplayMode \
	VRAMObserver \
	SpriteConverter \
	Blender \
	GLUtil

$(eval $(PROCESS_NODE))

