# $Id$

# on 22/8/2001 added the DLSB_FIRST to correct wrong Z80 compile

bin_PROGRAMS=openmsx
openmsx_SOURCES=openmsx.cc \
	system.h \
	msxconfig.hh msxconfig.cc \
	msxconfig_device.nn msxconfig_exception.nn \
	emutime.hh emutime.cc \
	msxexception.hh \
	MSXDevice.hh MSXDevice.cc \
	MSXMotherBoard.hh MSXMotherBoard.cc \
	MSXPPI.hh MSXPPI.cc \
	I8255.hh I8255.cc \
	MSXRom16KB.hh MSXRom16KB.cc \
	MSXSimple64KB.hh MSXSimple64KB.cc \
	MSXTMS9928a.hh MSXTMS9928a.cc \
	MSXZ80.hh MSXZ80.cc \
	devicefactory.hh devicefactory.cc \
	Scheduler.hh Scheduler.cc \
	Inputs.hh Inputs.cc \
	Z80.cc Z80.hh Z80DAA.h Z80Codes.h opc__Z80.h \
	E6Timer.hh E6Timer.cc \
	DummyDevice.hh DummyDevice.cc

# libxml++ and SDL
openmsx_LDADD= -lxml++ -lSDL -lpthread

# we want ALL warnings
AM_CXXFLAGS = -Wall -DDEBUG -g -DLSB_FIRST
