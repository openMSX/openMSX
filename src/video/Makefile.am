# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../commands \
	-I$(srcdir)/../config \
	-I$(srcdir)/../console \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../debugger \
	-I$(srcdir)/../events \
	-I$(srcdir)/../file \
	-I$(srcdir)/../settings \
	-I$(srcdir)/../thread \
	@XML_CFLAGS@ @SDL_CFLAGS@
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

noinst_LTLIBRARIES = libVideo.la
libVideo_la_SOURCES = \
	DisplayMode.hh \
	VDP.hh VDP.cc \
	VDPCmdEngine.hh VDPCmdEngine.cc \
	VDPVRAM.hh VDPVRAM.cc \
	VRAMObserver.hh \
	DirtyChecker.hh DirtyChecker.cc \
	VDPSettings.hh VDPSettings.cc \
	SpriteChecker.hh SpriteChecker.cc \
	RenderSettings.hh RenderSettings.cc \
	FrameSkipSetting.hh FrameSkipSetting.cc \
	Renderer.hh Renderer.cc \
	RendererFactory.hh RendererFactory.cc \
	Icon.hh Icon.cc \
	PixelRenderer.hh PixelRenderer.cc \
	DummyRenderer.hh DummyRenderer.cc \
	SDLRenderer.hh SDLRenderer.cc \
	SDLGLRenderer.hh SDLGLRenderer.cc \
	XRenderer.hh XRenderer.cc \
	BitmapConverter.hh BitmapConverter.cc \
	CharacterConverter.hh CharacterConverter.cc \
	SpriteConverter.hh \
	Blender.hh \
	Scalers.hh Scalers.cc \
	GLUtil.hh GLUtil.cc

libVideo_la_LIBADD =

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
