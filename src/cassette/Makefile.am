# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` -I$(srcdir)/.. -I$(srcdir)/../cpu -I$(srcdir)/../config -I$(srcdir)/../thread -I$(srcdir)/../file -I$(srcdir)/../ConsoleSource @XML_CFLAGS@ @SDL_CFLAGS@
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

noinst_LTLIBRARIES = libCassette.la
libCassette_la_SOURCES = \
	CassetteDevice.cc CassetteDevice.hh \
	CassettePlayer.cc CassettePlayer.hh \
	CassettePort.cc CassettePort.hh \
	DummyCassetteDevice.cc DummyCassetteDevice.hh \
	MSXTapePatch.cc MSXTapePatch.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
