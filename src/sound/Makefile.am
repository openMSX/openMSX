# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` -I$(srcdir)/.. -I$(srcdir)/../cpu -I$(srcdir)/../config @XML_CFLAGS@ @SDL_CFLAGS@
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
	Mixer.cc Mixer.hh \
	SoundDevice.cc SoundDevice.hh \
	MSXPSG.cc MSXPSG.hh \
	AY8910.cc AY8910.hh \
	DACSound.cc DACSound.hh \
	KeyClick.hh KeyClick.cc \
	SCC.cc SCC.hh \
	MSXSCCPlusCart.cc MSXSCCPlusCart.hh \
	MSXAudio.cc MSXAudio.hh \
	Y8950.cc Y8950.hh \
	Y8950Timer.cc Y8950Timer.hh \
	Y8950Adpcm.cc Y8950Adpcm.hh \
	Y8950KeyboardConnector.cc Y8950KeyboardConnector.hh \
	Y8950KeyboardDevice.cc Y8950KeyboardDevice.hh \
	MC6850.cc MC6850.hh \
	MSXFmPac.cc MSXFmPac.hh \
	MSXMusic.cc MSXMusic.hh \
	MSXYM2413.cc MSXYM2413.hh \
	YM2413.cc YM2413.hh
	

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
