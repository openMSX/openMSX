# $Id$

# we want most warnings

AM_CXXFLAGS_BASE = -Wall -pipe `test -f .debug && echo "-DDEBUG"` -I$(srcdir)/cpu -I$(srcdir)/sound -I$(srcdir)/config -I$(srcdir)/fdc @XML_CFLAGS@ @SDL_CFLAGS@
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

# or perhaps sometimes even more:
# AM_CXXFLAGS = -Wall -W -DDEBUG -g3 -pipe

SUBDIRS = cfg libxmlx ConsoleSource cpu sound config tools fdc

#bin_PROGRAMS=openmsx cfgtest
bin_PROGRAMS=openmsx

openmsx_SOURCES=\
	main.cc icon.nn \
	system.h openmsx.hh \
	EmuTime.hh EmuTime.cc \
	MSXException.hh \
	MSXDevice.hh MSXDevice.cc \
	MSXIODevice.hh MSXIODevice.cc MSXMemDevice.hh MSXMemDevice.cc \
	MSXMotherBoard.hh MSXMotherBoard.cc \
	MSXPPI.hh MSXPPI.cc I8255.hh I8255.cc \
	CassettePort.hh CassettePort.cc \
	CassetteDevice.hh CassetteDevice.cc \
	DummyCassetteDevice.hh DummyCassetteDevice.cc \
	CassettePlayer.hh CassettePlayer.cc \
	MSXSimple64KB.hh MSXSimple64KB.cc \
	VDP.hh VDP.cc \
	VDPCmdEngine.hh VDPCmdEngine.cc \
	VDPVRAM.hh VDPVRAM.cc \
	SpriteChecker.hh SpriteChecker.cc \
	Renderer.hh Renderer.cc \
	SDLHiRenderer.hh SDLHiRenderer.cc \
	SDLGLRenderer.hh SDLGLRenderer.cc \
	XRenderer.hh XRenderer.cc \
	BitmapConverter.hh BitmapConverter.cc \
	CharacterConverter.hh CharacterConverter.cc \
	PlatformFactory.hh PlatformFactory.cc \
	devicefactory.hh devicefactory.cc \
	Scheduler.hh Scheduler.cc Schedulable.hh Schedulable.cc \
	EventDistributor.hh EventDistributor.cc EventListener.hh \
	Thread.hh Thread.cc \
	Mutex.hh Mutex.cc \
	HotKey.hh HotKey.cc \
	Keyboard.hh Keyboard.cc \
	Leds.hh Leds.cc \
	MSXE6Timer.hh MSXE6Timer.cc \
	DummyDevice.hh DummyDevice.cc \
	JoystickPorts.hh JoystickPorts.cc \
	JoystickDevice.hh JoystickDevice.cc \
	DummyJoystick.hh DummyJoystick.cc \
	Joystick.hh Joystick.cc Mouse.hh Mouse.cc \
	JoyNet.hh JoyNet.cc \
	MSXPrinterPort.hh MSXPrinterPort.cc \
	PrinterPortDevice.hh PrinterPortDevice.cc \
	PrinterPortSimpl.hh PrinterPortSimpl.cc \
	PrinterPortLogger.cc PrinterPortLogger.hh \
	MSXKanji.hh MSXKanji.cc \
	MSXMemoryMapper.hh MSXMemoryMapper.cc MSXMapperIO.hh MSXMapperIO.cc \
	MSXMapperIOTurboR.hh MSXMapperIOTurboR.cc \
	MSXMapperIOPhilips.hh MSXMapperIOPhilips.cc \
	MSXRTC.hh MSXRTC.cc RP5C01.hh RP5C01.cc \
	RealTime.hh RealTime.cc \
	MSXRom.hh MSXRom.cc \
	KeyEventInserter.cc KeyEventInserter.hh \
	SDLEventInserter.cc SDLEventInserter.hh \
	MSXRomDevice.cc MSXRomDevice.hh \
	MSXRomPatchInterface.hh \
	MSXTapePatch.cc MSXTapePatch.hh \
	FileOpener.cc FileOpener.hh \
	FileManager.cc FileManager.hh \
	MSXARCdebug.cc MSXARCdebug.hh \
	CommandLineParser.cc CommandLineParser.hh \
	MSXDiskRomPatch.cc MSXDiskRomPatch.hh \
	atoll.c \
	PluggingController.hh PluggingController.cc \
	Connector.hh Connector.cc Pluggable.hh \
	config.h \
	File.cc File.hh FileType.hh \
	HttpFile.cc HttpFile.hh \
	LocalFile.cc LocalFile.hh

# Very much out of date:
#	SDLLoRenderer.hh SDLLoRenderer.cc

openmsx_LDADD = \
	config/libConfig.la \
	libxmlx/libxmlx.la \
	ConsoleSource/libSDL_console.la \
	cpu/libCPU.la \
	sound/libSound.la \
	fdc/libFdc.la \
	@XML_LIBS@ \
	@SDL_LIBS@

#cfgtest_SOURCES = MSXConfig.hh MSXConfig.cc \
#	cfgtest.cc EmuTime.hh \
#	XMLConfig.hh XMLConfig.cc \
#	openmsx.hh msxexception.hh
#
#cfgtest_LDADD = libxmlx/libxmlx.la

EXTRA_DIST = ConsoleFont.bmp

MAINTAINERCLEANFILES = Makefile.in config.h.in stamp-h.in

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f .debug
setdebug:
	@touch .debug
