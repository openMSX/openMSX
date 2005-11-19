# $Id$

include build/node-start.mk

SUBDIRS:= \
	v9990

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker \
	VideoSourceSetting \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLVideoSystem SDLRasterizer SDLSnow \
	DummyRenderer \
	XRenderer \
	BitmapConverter CharacterConverter \
	Scanline \
	Scaler LowScaler Scaler2 Scaler3 \
	SimpleScaler SaI2xScaler SaI3xScaler Scale2xScaler Scale3xScaler \
	HQ2xScaler HQ2xLiteScaler \
	HQ3xScaler HQ3xLiteScaler \
	RGBTriplet3xScaler \
	Simple3xScaler \
	ScreenShotSaver \
	VideoSystem Display VideoLayer \
	Layer \
	DummyVideoSystem \
	IconLayer \
	SDLImage \
	MemoryOps \
	RawFrame PostProcessor \
	OutputSurface SDLOutputSurface \
	Icon

HDR_ONLY:= \
	DirtyChecker \
	DisplayMode \
	VRAMObserver \
	SpriteConverter \
	PixelOperations \
	LineScalers \
	Rasterizer \
	FrameSource DeinterlacedFrame \
	HQCommon \
	VideoSystemChangeListener.hh

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLVideoSystem GLRasterizer GLSnow GLUtil GLImage \
	SDLGLOutputSurface

include build/node-end.mk

