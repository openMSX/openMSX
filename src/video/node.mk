# $Id$

include build/node-start.mk

SUBDIRS:= \
	v9990

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker ADVram \
	VideoSourceSetting \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLVideoSystem SDLRasterizer FBPostProcessor SDLSnow \
	DummyRenderer DummyRasterizer \
	BitmapConverter CharacterConverter \
	ScreenShotSaver \
	VideoSystem Display VideoLayer \
	Layer \
	DummyVideoSystem \
	SDLImage \
	FrameSource RawFrame DeinterlacedFrame DoubledFrame PostProcessor \
	OutputSurface VisibleSurface SDLVisibleSurface \
	Icon \
	AviRecorder AviWriter ZMBVEncoder \
	ScalerFactory \
	LowScaler

ifneq ($(MAX_SCALE_FACTOR), 1)
SRC_HDR += \
	Scanline \
	Scaler2 Scaler3 \
	SimpleScaler SaI2xScaler SaI3xScaler Scale2xScaler Scale3xScaler \
	HQ2xScaler HQ2xLiteScaler \
	HQ3xScaler HQ3xLiteScaler \
	RGBTriplet3xScaler \
	Simple3xScaler \
	Multiply32
endif

ifeq ($(PLATFORM_GP2X), 1)
SRC_HDR += \
	GP2XMMUHack
endif

HDR_ONLY:= \
	DirtyChecker \
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
	BaseImage

DIST:= \
	HQ2xScaler-1x1to2x2.nn \
	HQ2xScaler-1x1to1x2.nn \
	HQ3xScaler-1x1to3x3.nn \
	HQ2xLiteScaler-1x1to2x2.nn \
	HQ2xLiteScaler-1x1to1x2.nn \
	HQ3xLiteScaler-1x1to3x3.nn

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLVisibleSurface GLSnow GLUtil GLImage \
	GLPostProcessor GLScalerFactory \
	GLSimpleScaler GLScaleNxScaler GLSaIScaler GLTVScaler \
	GLRGBScaler GLHQScaler GLHQLiteScaler

HDR_ONLY_$(COMPONENT_GL)+= \
	GLScaler

include build/node-end.mk

