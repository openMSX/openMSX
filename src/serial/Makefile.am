# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../config \
	-I$(srcdir)/../events \
	-I$(srcdir)/../file \
	-I$(srcdir)/../memory \
	-I$(srcdir)/../thread \
	-I$(srcdir)/../debugger
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

noinst_LTLIBRARIES = libSerial.la
libSerial_la_SOURCES = \
	I8251.cc I8251.hh \
	I8254.cc I8254.hh \
	ClockPin.cc ClockPin.hh \
	SerialDataInterface.hh \
	MSXMidi.cc MSXMidi.hh \
	MidiInDevice.cc  MidiInDevice.hh \
	MidiOutDevice.cc MidiOutDevice.hh \
	DummyMidiInDevice.cc  DummyMidiInDevice.hh \
	DummyMidiOutDevice.cc DummyMidiOutDevice.hh \
	MidiInConnector.cc  MidiInConnector.hh \
	MidiOutConnector.cc MidiOutConnector.hh \
	MidiOutLogger.cc MidiOutLogger.hh \
	MidiInReader.cc MidiInReader.hh 

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
