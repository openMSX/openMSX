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

noinst_LTLIBRARIES = libFdc.la
libFdc_la_SOURCES = \
	FDCFactory.cc FDCFactory.hh \
	MSXFDC.cc MSXFDC.hh \
	PhilipsFDC.cc PhilipsFDC.hh \
	BrazilFDC.cc BrazilFDC.hh \
	NationalFDC.cc NationalFDC.hh \
	WD2793.cc WD2793.hh \
	DiskImageManager.cc DiskImageManager.hh \
	FDCBackEnd.cc FDCBackEnd.hh \
	FDCDummyBackEnd.cc FDCDummyBackEnd.hh \
	FDC_DSK.cc FDC_DSK.hh \
	FDC_XSA.cc FDC_XSA.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
