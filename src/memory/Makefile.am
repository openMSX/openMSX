# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../cassette \
	-I$(srcdir)/../config \
	-I$(srcdir)/../commands \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../events \
	-I$(srcdir)/../fdc \
	-I$(srcdir)/../file \
	-I$(srcdir)/../sound \
	-I$(srcdir)/../thread \
	-I$(srcdir)/../debugger \
	-I$(srcdir)/../settings \
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

noinst_LTLIBRARIES = libMemory.la
libMemory_la_SOURCES = \
	MSXMapperIO.cc MSXMapperIO.hh \
	MSXMapperIOPhilips.cc MSXMapperIOPhilips.hh \
	MSXMapperIOTurboR.cc MSXMapperIOTurboR.hh \
	MSXMemoryMapper.cc MSXMemoryMapper.hh \
	MSXRam.cc MSXRam.hh \
	MSXRomCLI.cc MSXRomCLI.hh \
	SRAM.hh SRAM.cc \
	Rom.cc Rom.hh \
	RomInfo.cc RomInfo.hh \
	RomTypes.hh \
	MSXRom.cc MSXRom.hh \
	Rom4kBBlocks.cc Rom4kBBlocks.hh \
	Rom8kBBlocks.cc Rom8kBBlocks.hh \
	Rom16kBBlocks.cc Rom16kBBlocks.hh \
	RomPlain.cc RomPlain.hh \
	RomPageNN.cc RomPageNN.hh \
	RomGeneric8kB.cc RomGeneric8kB.hh \
	RomGeneric16kB.cc RomGeneric16kB.hh \
	RomKonami4.cc RomKonami4.hh \
	RomKonami5.cc RomKonami5.hh \
	RomAscii8kB.cc RomAscii8kB.hh \
	RomAscii8_8.cc RomAscii8_8.hh \
	RomAscii16kB.cc RomAscii16kB.hh \
	RomHydlide2.cc RomHydlide2.hh \
	RomRType.cc RomRType.hh \
	RomCrossBlaim.cc RomCrossBlaim.hh \
	RomMSXAudio.cc RomMSXAudio.hh \
	RomHarryFox.cc RomHarryFox.hh \
	RomGameMaster2.cc RomGameMaster2.hh \
	RomPanasonic.cc RomPanasonic.hh \
	RomNational.cc RomNational.hh \
	RomMajutsushi.cc RomMajutsushi.hh \
	RomSynthesizer.cc RomSynthesizer.hh \
	RomHalnote.cc RomHalnote.hh \
	RomKorean80in1.cc RomKorean80in1.hh \
	RomKorean90in1.cc RomKorean90in1.hh \
	RomKorean126in1.cc RomKorean126in1.hh \
	RomHolyQuran.cc RomHolyQuran.hh \
	RomFSA1FM.cc RomFSA1FM.hh \
	RomFactory.cc RomFactory.hh \
	PanasonicMemory.cc PanasonicMemory.hh \
	PanasonicRam.cc PanasonicRam.hh \
	PanasonicRom.cc PanasonicRom.hh \
	MSXMegaRam.cc MSXMegaRam.hh \
	MSXPac.cc MSXPac.hh \
	MSXHBI55.cc MSXHBI55.hh

libMemory_la_LIBADD =

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
