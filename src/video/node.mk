# $Id$

include build/node-start.mk

SUBDIRS:= \
	v9990 ld

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
	DeinterlacedFrame DoubledFrame SuperImposedFrame \
	OutputSurface VisibleSurface SDLVisibleSurface \
	SDLOffScreenSurface \
	Icon \
	AviRecorder AviWriter ZMBVEncoder \
	ScalerFactory \
	Scaler1 Transparent1xScaler

ifneq ($(MAX_SCALE_FACTOR), 1)
SRC_HDR += \
	Scanline \
	Scaler2 Scaler3 \
	Simple2xScaler Simple3xScaler \
	SaI2xScaler SaI3xScaler \
	Scale2xScaler Scale3xScaler \
	HQ2xScaler HQ2xLiteScaler \
	HQ3xScaler HQ3xLiteScaler \
	RGBTriplet3xScaler \
	Transparent2xScaler Transparent3xScaler \
	Multiply32
endif

ifeq ($(PLATFORM_GP2X), 1)
SRC_HDR += \
	GP2XMMUHack
endif

HDR_ONLY:= \
	DisplayMode \
	VRAMObserver \
	SpriteConverter \
	PixelOperations \
	LineScalers \
	Scaler \
	Rasterizer \
	HQCommon \
	VideoSystemChangeListener \
	LayerListener \
	VideoSource \
	SDLSurfacePtr

DIST:= \
	HQ2xScaler-1x1to2x2.nn \
	HQ2xScaler-1x1to1x2.nn \
	HQ3xScaler-1x1to3x3.nn \
	HQ2xLiteScaler-1x1to2x2.nn \
	HQ2xLiteScaler-1x1to1x2.nn \
	HQ3xLiteScaler-1x1to3x3.nn

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLOutputSurface SDLGLVisibleSurface SDLGLOffScreenSurface \
	GLSnow GLUtil GLImage \
	GLPostProcessor GLScalerFactory GLScaler \
	GLSimpleScaler GLScaleNxScaler GLSaIScaler GLTVScaler \
	GLRGBScaler GLHQScaler GLHQLiteScaler

include build/node-end.mk

