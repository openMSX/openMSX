# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../config \
	-I$(srcdir)/../events \
	-I$(srcdir)/../file \
	-I$(srcdir)/../memory \
	-I$(srcdir)/../thread \
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

noinst_LTLIBRARIES = libFdc.la
libFdc_la_SOURCES = \
	FDCFactory.cc FDCFactory.hh \
	MSXFDC.cc MSXFDC.hh \
	WD2793BasedFDC.cc WD2793BasedFDC.hh \
	PhilipsFDC.cc PhilipsFDC.hh \
	NationalFDC.cc NationalFDC.hh \
	MicrosolFDC.cc MicrosolFDC.hh \
	WD2793.cc WD2793.hh \
	TurboRFDC.cc TurboRFDC.hh \
	TC8566AF.cc TC8566AF.hh \
	DiskImageCLI.cc DiskImageCLI.hh \
	DiskDrive.cc DiskDrive.hh \
	DriveMultiplexer.cc DriveMultiplexer.hh \
	FDCBackEnd.cc FDCBackEnd.hh \
	FDCDummyBackEnd.cc FDCDummyBackEnd.hh \
	FDC_DSK.cc FDC_DSK.hh \
	FDC_XSA.cc FDC_XSA.hh \
	CRC16.hh CRC16.cc 

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
