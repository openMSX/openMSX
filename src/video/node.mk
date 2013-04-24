include build/node-start.mk

SUBDIRS:= \
	v9990 ld scalers

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker ADVram \
	VideoSourceSetting \
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

ifeq ($(PLATFORM_GP2X), 1)
SRC_HDR += \
	GP2XMMUHack
endif

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

DIST:= \
	FBPostProcessor-x64.asm \
	FBPostProcessor-x86.asm

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLOutputSurface SDLGLVisibleSurface SDLGLOffScreenSurface \
	GLSnow GLUtil GLImage GLPostProcessor

include build/node-end.mk
