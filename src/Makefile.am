# $Id$

# we want most warnings

AM_CXXFLAGS_BASE = -Wall -pipe `test -f .debug && echo "-DDEBUG"`
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

SUBDIRS = cfg libxmlx ConsoleSource

#bin_PROGRAMS=openmsx cfgtest
bin_PROGRAMS=openmsx

openmsx_SOURCES=openmsx.cc icon.nn \
	system.h openmsx.hh \
	MSXConfig.cc MSXConfig.hh \
	XMLConfig.cc XMLConfig.hh \
	EmuTime.hh EmuTime.cc \
	msxexception.hh \
	MSXDevice.hh MSXDevice.cc \
	MSXIODevice.hh MSXIODevice.cc MSXMemDevice.hh MSXMemDevice.cc \
	MSXMotherBoard.hh MSXMotherBoard.cc \
	MSXPPI.hh MSXPPI.cc I8255.hh I8255.cc \
	KeyClick.hh KeyClick.cc \
	CassettePort.hh CassettePort.cc \
	CassetteDevice.hh DummyCassetteDevice.hh DummyCassetteDevice.cc \
	CassettePlayer.hh CassettePlayer.cc \
	MSXRom16KB.hh MSXRom16KB.cc \
	MSXSimple64KB.hh MSXSimple64KB.cc \
	VDP.hh VDP.cc \
	VDPCmdEngine.hh VDPCmdEngine.cc \
	Renderer.hh Renderer.cc \
	SDLLoRenderer.hh SDLLoRenderer.cc \
	SDLHiRenderer.hh SDLHiRenderer.cc \
	MSXCPU.hh MSXCPU.cc \
	CPU.hh CPU.cc CPU.ii CPUInterface.hh CPUInterface.cc \
	CPUCore.n1 CPUCore.n2 CPUTables.nn \
	Z80.cc Z80.hh Z80Tables.nn Z80Dasm.h \
	devicefactory.hh devicefactory.cc \
	Scheduler.hh Scheduler.cc \
	EventDistributor.hh EventDistributor.cc \
	Thread.hh Thread.cc \
	Mutex.hh Mutex.cc \
	HotKey.hh HotKey.cc \
	Keyboard.hh Keyboard.cc \
	Leds.hh Leds.cc \
	MSXE6Timer.hh MSXE6Timer.cc \
	DummyDevice.hh DummyDevice.cc \
	Mixer.hh Mixer.cc SoundDevice.hh SoundDevice.cc \
	MSXPSG.hh MSXPSG.cc AY8910.hh AY8910.cc \
	YM2413.hh YM2413.cc MSXYM2413.hh MSXYM2413.cc\
	MSXMusic.hh MSXMusic.cc MSXFmPac.hh MSXFmPac.cc \
	Y8950.hh Y8950.cc MSXAudio.hh MSXAudio.cc \
	JoystickPorts.hh JoystickPorts.cc JoystickDevice.hh \
	DummyJoystick.hh DummyJoystick.cc \
	Joystick.hh Joystick.cc Mouse.hh Mouse.cc \
	MSXPrinterPort.hh MSXPrinterPort.cc PrinterPortDevice.hh \
	MSXKanji.hh MSXKanji.cc \
	MSXMemoryMapper.hh MSXMemoryMapper.cc MSXMapperIO.hh MSXMapperIO.cc \
	MSXMapperIOTurboR.hh MSXMapperIOTurboR.cc \
	MSXMapperIOPhilips.hh MSXMapperIOPhilips.cc \
	MSXRTC.hh MSXRTC.cc RP5C01.hh RP5C01.cc \
	RealTime.hh RealTime.cc \
	MSXGameCartridge.hh MSXGameCartridge.cc \
	DACSound.cc DACSound.hh \
	KeyEventInserter.cc KeyEventInserter.hh \
	SDLEventInserter.cc SDLEventInserter.hh \
	LoadFile.hh LoadFile.cc \
	SCC.hh SCC.cc \
	MSXSCCPlusCart.hh MSXSCCPlusCart.cc \
	MSXRom.cc MSXRom.hh \
	MSXRomPatchInterface.hh MSXRomPatchInterface.cc \
	MSXTapePatch.cc MSXTapePatch.hh \
	FileOpener.cc FileOpener.hh \
	Console.cc Console.hh \
	MSXARCdebug.cc MSXARCdebug.hh \
	MSXDiskRomPatch.cc MSXDiskRomPatch.hh

# uncomment if you want to link with libxmlx
# doesn't do anything yet at the moment
openmsx_LDADD = libxmlx/libxmlx.la ConsoleSource/libSDL_console.a
#openmsx_LDADD = ConsoleSource/libSDL_console.a

# link libxmlx static, all external libs are still
# linked dynamicaly, unfortunately, this links more stuff
# static, soo leave it in comments for now
# openmsx_LDFLAGS = -static

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
