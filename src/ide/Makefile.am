# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../config \
	-I$(srcdir)/../commands \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../file \
	-I$(srcdir)/../memory \
	-I$(srcdir)/../thread \
	-I$(srcdir)/../events \
	-I$(srcdir)/../debugger \
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

noinst_LTLIBRARIES = libIDE.la
libIDE_la_SOURCES = \
	SunriseIDE.cc SunriseIDE.hh \
	IDEDevice.hh DummyIDEDevice.cc DummyIDEDevice.hh \
	IDEDeviceFactory.cc IDEDeviceFactory.hh \
	IDEHD.cc IDEHD.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
