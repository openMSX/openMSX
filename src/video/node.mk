# $Id$

include build/node-start.mk

SUBDIRS:= \
	v9990

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker ADVram \
	VideoSourceSetting \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLVideoSystem SDLRasterizer FBPostProcessor SDLSnow \
	DummyRenderer \
	BitmapConverter CharacterConverter \
	Scanline \
	ScalerFactory \
	LowScaler Scaler2 Scaler3 \
	SimpleScaler SaI2xScaler SaI3xScaler Scale2xScaler Scale3xScaler \
	HQ2xScaler HQ2xLiteScaler \
	HQ3xScaler HQ3xLiteScaler \
	RGBTriplet3xScaler \
	Simple3xScaler \
	Multiply32 \
	ScreenShotSaver \
	VideoSystem Display VideoLayer \
	Layer \
	DummyVideoSystem \
	IconLayer IconStatus \
	SDLImage \
	MemoryOps \
	FrameSource RawFrame DeinterlacedFrame DoubledFrame PostProcessor \
	OutputSurface VisibleSurface SDLVisibleSurface \
	Icon \
	AviRecorder AviWriter ZMBVEncoder

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
	LayerListener

DIST:= \
	HQ2xScaler-1x1to2x2.nn \
	HQ2xScaler-1x1to1x2.nn \
	HQ3xScaler-1x1to3x3.nn \
	HQ2xLiteScaler-1x1to2x2.nn \
	HQ2xLiteScaler-1x1to1x2.nn \
	HQ3xLiteScaler-1x1to3x3.nn

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLVisibleSurface GLSnow GLUtil GLImage \
	GLRasterizer GLPostProcessor GLScalerFactory \
	GLSimpleScaler GLScaleNxScaler GLTVScaler \
	GLHQScaler GLHQLiteScaler

HDR_ONLY_$(COMPONENT_GL)+= \
	GLScaler

include build/node-end.mk

