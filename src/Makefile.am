# $Id$

bin_PROGRAMS=openmsx
openmsx_SOURCES=openmsx.cc \
	system.h \
	msxconfig.hh msxconfig.cc \
	emutime.hh emutime.cc \
	msxexception.hh \
	MSXDevice.hh MSXDevice.cc \
	MSXMotherBoard.hh MSXMotherBoard.cc linkedlist.hh \
	MSXPPI.hh MSXPPI.cc \
	MSXRom16KB.hh MSXRom16KB.cc \
	MSXSimple64KB.hh MSXSimple64KB.cc \
	MSXZ80.hh MSXZ80.cc \
	Scheduler.hh Scheduler.cc \
	Z80.cc Z80.hh Z80DAA.h Z80Codes.h opc__Z80.h

# libxml++ and SDL
openmsx_LDADD= -lxml++ -lSDL

# we want ALL warnings
AM_CXXFLAGS = -Wall
