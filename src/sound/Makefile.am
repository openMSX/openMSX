# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` -fhuge-objects -I$(srcdir)/.. -I$(srcdir)/../cpu -I$(srcdir)/../config @XML_CFLAGS@ @SDL_CFLAGS@
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

noinst_LTLIBRARIES = libSound.la
libSound_la_SOURCES = \
	AY8910.cc AY8910.hh \
	DACSound.cc DACSound.hh \
	KeyClick.hh KeyClick.cc \
	MSXAudio.cc MSXAudio.hh \
	MSXFmPac.cc MSXFmPac.hh \
	MSXMusic.cc MSXMusic.hh \
	MSXPSG.cc MSXPSG.hh \
	MSXSCCPlusCart.cc MSXSCCPlusCart.hh \
	MSXYM2413.cc MSXYM2413.hh \
	Mixer.cc Mixer.hh \
	SCC.cc SCC.hh \
	SoundDevice.cc SoundDevice.hh \
	Y8950.cc Y8950.hh \
	YM2413.cc YM2413.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
