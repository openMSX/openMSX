# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../config \
	-I$(srcdir)/../events \
	-I$(srcdir)/../file \
	-I$(srcdir)/../memory \
	-I$(srcdir)/../thread
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
	I8254.cc I8254.hh \
	ClockPin.cc ClockPin.hh \
	MSXMidi.cc MSXMidi.hh
	

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
