include build/node-start.mk

SUBDIRS:= \
	v9990 ld scalers

SRC_HDR:= \
	VDP VDPAccessSlots VDPCmdEngine VDPVRAM SpriteChecker ADVram \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLVideoSystem SDLRasterizer FBPostProcessor SDLSnow \
	DummyRenderer \
	BitmapConverter CharacterConverter \
	PNG \
	VideoSystem Display VideoLayer \
	Layer \
	DummyVideoSystem \
	SDLImage BaseImage \
	FrameSource RawFrame PostProcessor \
	DeinterlacedFrame DoubledFrame \
	SuperImposedFrame SuperImposedVideoFrame \
	OutputSurface VisibleSurface SDLVisibleSurface \
	SDLOffScreenSurface \
	Icon \
	AviRecorder AviWriter ZMBVEncoder

HDR_ONLY:= \
	DisplayMode \
	VRAMObserver \
	SpriteConverter \
	PixelOperations \
	Rasterizer \
	VideoSystemChangeListener \
	LayerListener \
	SDLSurfacePtr \
	OutputRectangle

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLOutputSurface SDLGLVisibleSurface SDLGLOffScreenSurface \
	GLSnow GLUtil GLImage GLPostProcessor

include build/node-end.mk
