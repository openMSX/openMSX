# $Id$

include build/node-start.mk

SUBDIRS:= \
	v9990

SRC_HDR:= \
	VDP VDPCmdEngine VDPVRAM SpriteChecker \
	VideoSourceSetting \
	Renderer RendererFactory RenderSettings PixelRenderer \
	SDLVideoSystem SDLRasterizer SDLSnow \
	SDLUtil \
	DummyRenderer \
	XRenderer \
	BitmapConverter CharacterConverter \
	Scaler LowScaler Scaler2 Scaler3 \
	SimpleScaler SaI2xScaler Scale2xScaler \
	HQ2xScaler HQ2xLiteScaler \
	HQ3xScaler HQ3xLiteScaler \
	ScreenShotSaver \
	VideoSystem Display VideoLayer \
	Layer \
	DummyVideoSystem \
	IconLayer \
	SDLImage \
	MemoryOps \
	RawFrame PostProcessor \
	Icon

HDR_ONLY:= \
	DirtyChecker \
	DisplayMode \
	VRAMObserver \
	SpriteConverter \
	Blender \
	Rasterizer \
	FrameSource \
	HQCommon

SRC_HDR_$(COMPONENT_GL)+= \
	SDLGLVideoSystem GLRasterizer GLSnow GLUtil GLImage

include build/node-end.mk

