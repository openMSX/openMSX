# $Id$

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
	MSXRom16KB.hh MSXRom16KB.cc \
	MSXSimple64KB.hh MSXSimple64KB.cc \
	MSXZ80.hh MSXZ80.cc \
	devicefactory.hh devicefactory.cc \
	Scheduler.hh Scheduler.cc \
	Inputs.hh Inputs.cc \
	Z80.cc Z80.hh Z80DAA.h Z80Codes.h opc__Z80.h
#	sortedlist.hh sortedlist.cc

# libxml++ and SDL
openmsx_LDADD= -lxml++ -lSDL

# we want ALL warnings
AM_CXXFLAGS = -Wall -DDEBUG -g
