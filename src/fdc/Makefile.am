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
	Disk.cc Disk.hh \
	SectorBasedDisk.cc SectorBasedDisk.hh \
	DummyDisk.cc DummyDisk.hh \
	DSKDiskImage.cc DSKDiskImage.hh \
	XSADiskImage.cc XSADiskImage.hh \
	CRC16.hh CRC16.cc \
	FDC_DirAsDSK.cc FDC_DirAsDSK.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
