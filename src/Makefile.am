# $Id$

# we want most warnings

# FIXME: -lSDL_image shouldn't be needed
AM_CXXFLAGS_BASE = -Wall -pipe `test -f .debug && echo "-DDEBUG"` -I$(srcdir)/cpu -I$(srcdir)/sound -I$(srcdir)/config -I$(srcdir)/fdc -I$(srcdir)/file -I$(srcdir)/ide -I$(srcdir)/thread -I$(srcdir)/ConsoleSource -I$(srcdir)/cassette -I$(srcdir)/events @XML_CFLAGS@ @SDL_CFLAGS@ -lSDL_image
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

SUBDIRS = cfg libxmlx ConsoleSource cpu sound config tools fdc file ide thread cassette events

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
	MSXRam.hh MSXRam.cc \
	VDP.hh VDP.cc \
	VDPCmdEngine.hh VDPCmdEngine.cc \
	VDPVRAM.hh VDPVRAM.cc \
	VDPSettings.hh VDPSettings.cc \
	SpriteChecker.hh SpriteChecker.cc \
	RenderSettings.hh RenderSettings.cc \
	Renderer.hh Renderer.cc \
	PixelRenderer.hh PixelRenderer.cc \
	SDLLoRenderer.hh SDLLoRenderer.cc \
	SDLHiRenderer.hh SDLHiRenderer.cc \
	SDLGLRenderer.hh SDLGLRenderer.cc \
	XRenderer.hh XRenderer.cc \
	BitmapConverter.hh BitmapConverter.cc \
	CharacterConverter.hh CharacterConverter.cc \
	PlatformFactory.hh PlatformFactory.cc \
	DeviceFactory.hh DeviceFactory.cc \
	Scheduler.hh Scheduler.cc Schedulable.hh Schedulable.cc \
	Keyboard.hh Keyboard.cc \
	Leds.hh Leds.cc \
	MSXE6Timer.hh MSXE6Timer.cc \
	MSXF4Device.hh MSXF4Device.cc \
	MSXTurboRLeds.hh MSXTurboRLeds.cc \
	MSXTurboRPause.hh MSXTurboRPause.cc \
	MSXS1985.hh MSXS1985.cc \
	MSXS1990.hh MSXS1990.cc \
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
	MSXKanji12.hh MSXKanji12.cc \
	MSXBunsetsu.hh MSXBunsetsu.cc \
	MSXMemoryMapper.hh MSXMemoryMapper.cc MSXMapperIO.hh MSXMapperIO.cc \
	MSXMapperIOTurboR.hh MSXMapperIOTurboR.cc \
	MSXMapperIOPhilips.hh MSXMapperIOPhilips.cc \
	MSXRTC.hh MSXRTC.cc RP5C01.hh RP5C01.cc \
	RealTime.hh RealTime.cc \
	CartridgeSlotManager.hh CartridgeSlotManager.cc \
	SRAM.hh SRAM.cc \
	MSXDeviceSwitch.hh MSXDeviceSwitch.cc \
	MSXMatsushita.hh MSXMatsushita.cc \
	MSXRom.hh MSXRom.cc \
	RomTypes.hh RomTypes.cc \
	MSXRomDevice.cc MSXRomDevice.hh \
	MSXRomPatchInterface.hh \
	CommandLineParser.cc CommandLineParser.hh \
	MSXDiskRomPatch.cc MSXDiskRomPatch.hh \
	atoll.c \
	md5.cc md5.hh \
	PluggingController.hh PluggingController.cc \
	Connector.hh Connector.cc \
	Pluggable.hh Pluggable.cc \
	Settings.hh Settings.cc \
	config.h

# Very much out of date:
#	SDLLoRenderer.hh SDLLoRenderer.cc

openmsx_LDADD = \
	config/libConfig.la \
	libxmlx/libxmlx.la \
	ConsoleSource/libSDL_console.la \
	cpu/libCPU.la \
	sound/libSound.la \
	fdc/libFdc.la \
	file/libFile.la \
	ide/libIDE.la \
	thread/libThread.la \
	cassette/libCassette.la \
	events/libEvents.la \
	@XML_LIBS@ \
	@SDL_LIBS@

#cfgtest_SOURCES = MSXConfig.hh MSXConfig.cc \
#	cfgtest.cc EmuTime.hh \
#	XMLConfig.hh XMLConfig.cc \
#	openmsx.hh msxexception.hh
#
#cfgtest_LDADD = libxmlx/libxmlx.la

EXTRA_DIST = ConsoleFont.png romdb.dtd romdb.xml

MAINTAINERCLEANFILES = Makefile.in config.h.in stamp-h.in

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f .debug
setdebug:
	@touch .debug
