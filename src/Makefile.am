# $Id$

# we want most warnings

AM_CXXFLAGS_BASE = -Wall -pipe `test -f .debug && echo "-DDEBUG"` \
	-I$(srcdir)/cassette \
	-I$(srcdir)/config \
	-I$(srcdir)/console \
	-I$(srcdir)/cpu \
	-I$(srcdir)/debugger \
	-I$(srcdir)/events \
	-I$(srcdir)/fdc \
	-I$(srcdir)/file \
	-I$(srcdir)/ide \
	-I$(srcdir)/input \
	-I$(srcdir)/memory \
	-I$(srcdir)/serial \
	-I$(srcdir)/settings \
	-I$(srcdir)/sound \
	-I$(srcdir)/thread \
	-I$(srcdir)/video \
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

# or perhaps sometimes even more:
# AM_CXXFLAGS = -Wall -W -DDEBUG -g3 -pipe

SUBDIRS = \
	cassette config console cpu debugger events fdc file ide input \
	libxmlx memory serial settings sound thread video

#bin_PROGRAMS=openmsx cfgtest
bin_PROGRAMS=openmsx

openmsx_SOURCES=\
	main.cc Icon.hh Icon.cc \
	openmsx.hh util.hh \
	EmuTime.hh EmuTime.cc \
	MSXException.hh \
	MSXDevice.hh MSXDevice.cc \
	MSXIODevice.hh MSXIODevice.cc MSXMemDevice.hh MSXMemDevice.cc \
	MSXMotherBoard.hh MSXMotherBoard.cc \
	MSXPPI.hh MSXPPI.cc I8255.hh I8255.cc \
	PlatformFactory.hh PlatformFactory.cc \
	DeviceFactory.hh DeviceFactory.cc \
	Scheduler.hh Scheduler.cc Schedulable.hh Schedulable.cc \
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
	JoyNet.hh JoyNet.cc \
	MSXPrinterPort.hh MSXPrinterPort.cc \
	PrinterPortDevice.hh PrinterPortDevice.cc \
	PrinterPortSimpl.hh PrinterPortSimpl.cc \
	PrinterPortLogger.cc PrinterPortLogger.hh \
	MSXKanji.hh MSXKanji.cc \
	MSXKanji12.hh MSXKanji12.cc \
	MSXBunsetsu.hh MSXBunsetsu.cc \
	MSXRTC.hh MSXRTC.cc RP5C01.hh RP5C01.cc \
	RealTime.hh RealTime.cc \
	RealTimeSDL.hh RealTimeSDL.cc \
	RealTimeRTC.hh RealTimeRTC.cc \
	CartridgeSlotManager.hh CartridgeSlotManager.cc \
	MSXDeviceSwitch.hh MSXDeviceSwitch.cc \
	MSXMatsushita.hh MSXMatsushita.cc \
	FrontSwitch.hh FrontSwitch.cc \
	MSXRomPatchInterface.hh \
	CommandLineParser.cc CommandLineParser.hh \
	CliExtension.cc CliExtension.hh \
	MSXDiskRomPatch.cc MSXDiskRomPatch.hh \
	md5.cc md5.hh sha1.cc sha1.hh \
	CircularBuffer.hh CircularBuffer.cc \
	PluggingController.hh PluggingController.cc \
	Connector.hh Connector.cc \
	Pluggable.hh Pluggable.cc \
	DebugDevice.hh DebugDevice.cc \
	config.h

# When editing this list, make sure the following sequences are maintained:
#   memory < cassette
#   video < console
#   video < cpu
#   video < debugger
#   cpu < debugger
# TODO: Get rid of this manual dependency resolution.
openmsx_LDADD = \
	memory/libMemory.la \
	video/libVideo.la \
	cpu/libCPU.la \
	\
	cassette/libCassette.la \
	config/libConfig.la \
	console/libConsole.la \
	debugger/libDebugger.la \
	events/libEvents.la \
	fdc/libFdc.la \
	file/libFile.la \
	ide/libIDE.la \
	input/libInput.la \
	libxmlx/libxmlx.la \
	serial/libSerial.la \
	settings/libSettings.la \
	sound/libSound.la \
	thread/libThread.la \
	@XML_LIBS@ \
	-lSDL_image @SDL_LIBS@

#cfgtest_SOURCES = MSXConfig.hh MSXConfig.cc \
#	cfgtest.cc EmuTime.hh \
#	XMLConfig.hh XMLConfig.cc \
#	openmsx.hh msxexception.hh
#
#cfgtest_LDADD = libxmlx/libxmlx.la

MAINTAINERCLEANFILES = Makefile.in config.h.in stamp-h.in

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f .debug
setdebug:
	@touch .debug
