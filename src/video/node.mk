# $Id$

include build/node-start.mk

SUBDIRS:= \
	v9990

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker ADVram \
	VideoSourceSetting \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLVideoSystem SDLRasterizer SDLSnow \
	DummyRenderer \
	XRenderer \
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
	Icon

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
	VideoSystemChangeListener

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLVideoSystem GLRasterizer GLSnow GLUtil GLImage \
	SDLGLVisibleSurface

include build/node-end.mk

