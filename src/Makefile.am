# $Id$

# on 22/8/2001 added the DLSB_FIRST to correct wrong Z80 compile

SUBDIRS = cfg

bin_PROGRAMS=openmsx
openmsx_SOURCES=openmsx.cc \
	system.h openmsx.hh \
	msxconfig.hh msxconfig.cc \
	msxconfig_device.nn msxconfig_exception.nn \
	msxconfig_config.nn \
	xmlhelper.ii xmlhelper.hh \
	emutime.hh emutime.cc \
	msxexception.hh \
	MSXDevice.hh MSXDevice.cc \
	MSXMotherBoard.hh MSXMotherBoard.cc \
	MSXPPI.hh MSXPPI.cc I8255.hh I8255.cc \
	KeyClick.hh KeyClick.cc \
	MSXRom16KB.hh MSXRom16KB.cc \
	MSXSimple64KB.hh MSXSimple64KB.cc \
	MSXTMS9928a.hh MSXTMS9928a.cc \
	Renderer.hh \
	SDLLoRenderer.hh SDLLoRenderer.cc \
	MSXCPU.hh MSXCPU.cc \
	CPU.hh CPU.cc CPUInterface.hh CPUInterface.cc \
	Z80.cc Z80.hh Z80Codes.h opc_R800.h Z80Core.hh \
	Z80Tables.hh Z80Dasm.h \
	devicefactory.hh devicefactory.cc \
	Scheduler.hh Scheduler.cc \
	EventDistributor.hh EventDistributor.cc \
	HotKey.hh HotKey.cc \
	Keyboard.hh Keyboard.cc \
	Leds.hh Leds.cc \
	MSXE6Timer.hh MSXE6Timer.cc \
	DummyDevice.hh DummyDevice.cc \
	Mixer.hh Mixer.cc SoundDevice.hh SoundDevice.cc \
	MSXPSG.hh MSXPSG.cc AY8910.hh AY8910.cc \
	MSXMusic.hh MSXMusic.cc YM2413.hh YM2413.cc \
	JoystickPorts.hh JoystickPorts.cc JoystickDevice.hh \
	DummyJoystick.hh DummyJoystick.cc \
	Joystick.hh Joystick.cc Mouse.hh Mouse.cc \
	MSXPrinterPort.hh MSXPrinterPort.cc PrinterPortDevice.hh \
	MSXKanji.hh MSXKanji.cc \
	MSXMemoryMapper.hh MSXMemoryMapper.cc \
	MSXMapperIO.hh MSXMapperIO.cc \
	MSXMapperIOTurboR.hh MSXMapperIOTurboR.cc \
	MSXMapperIOPhilips.hh MSXMapperIOPhilips.cc \
	MSXRTC.hh MSXRTC.cc RP5C01.hh RP5C01.cc \
	MSXRealTime.hh MSXRealTime.cc \
	MSXMegaRom.hh MSXMegaRom.cc \
	MSXKonamiSynthesizer.hh  MSXKonamiSynthesizer.cc \
	DACSound.cc DACSound.hh \
	KeyEventInserter.cc KeyEventInserter.hh \
	SDLEventInserter.cc SDLEventInserter.hh
#	MSXPostLoad.hh MSXPostLoad.cc


# libxml++ and SDL
#openmsx_LDADD= -lxml++ $(SDL_LIBS) -lpthread
#I have a gcc-3.0 compatable libxml++ in /usr/local:
#openmsx_LDADD= /usr/local/lib/libxml++.la -lSDL -lpthread

# we want most warnings
AM_CXXFLAGS = -Wall -DDEBUG -g3 -fno-rtti -pipe
#AM_CXXFLAGS = -Wall -DDEBUG -g3 -pipe

# or perhaps sometimes even more:
# AM_CXXFLAGS = -Wall -W -DDEBUG -g3 -fno-rtti -pipe

MAINTAINERCLEANFILES = Makefile.in config.h.in stamp-h.in
