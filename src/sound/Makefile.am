# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../cassette \
	-I$(srcdir)/../config \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../events \
	-I$(srcdir)/../file \
	-I$(srcdir)/../memory \
	-I$(srcdir)/../thread \
	-I$(srcdir)/../serial \
	-I$(srcdir)/../settings \
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

noinst_LTLIBRARIES = libSound.la
libSound_la_SOURCES = \
	Mixer.cc Mixer.hh \
	SoundDevice.cc SoundDevice.hh \
	VolumeSetting.cc VolumeSetting.hh \
	MSXPSG.cc MSXPSG.hh \
	AY8910.cc AY8910.hh \
	DACSound16S.cc DACSound16S.hh \
	DACSound8U.cc DACSound8U.hh \
	KeyClick.hh KeyClick.cc \
	SCC.cc SCC.hh \
	MSXSCCPlusCart.cc MSXSCCPlusCart.hh \
	MSXAudio.cc MSXAudio.hh \
	Timer.cc Timer.hh \
	Y8950.cc Y8950.hh \
	Y8950Adpcm.cc Y8950Adpcm.hh \
	Y8950KeyboardConnector.cc Y8950KeyboardConnector.hh \
	Y8950KeyboardDevice.cc Y8950KeyboardDevice.hh \
	MC6850.cc MC6850.hh \
	MSXFmPac.cc MSXFmPac.hh \
	MSXMusic.cc MSXMusic.hh \
	MSXMusic_2.cc MSXMusic_2.hh \
	YM2413.cc YM2413.hh \
	YM2413_2.cc YM2413_2.hh \
	MSXTurboRPCM.cc MSXTurboRPCM.hh \
	YMF262.cc YMF262.hh \
	YMF278.cc YMF278.hh \
	MSXMoonSound.cc MSXMoonSound.hh \
	AudioInputConnector.cc AudioInputConnector.hh \
	AudioInputDevice.cc AudioInputDevice.hh \
	DummyAudioInputDevice.cc DummyAudioInputDevice.hh \
	WavAudioInput.cc WavAudioInput.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
