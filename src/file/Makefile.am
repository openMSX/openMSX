# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../config \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../events \
	-I$(srcdir)/../thread \
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

noinst_LTLIBRARIES = libFile.la
libFile_la_SOURCES = \
	File.cc File.hh \
	FileContext.cc FileContext.hh \
	FileException.hh \
	FileBase.cc FileBase.hh \
	LocalFile.cc LocalFile.hh \
	FileOperations.cc FileOperations.hh \
	CompressedFileAdapter.cc CompressedFileAdapter.hh \
	GZFileAdapter.cc GZFileAdapter.hh \
	ZipFileAdapter.cc ZipFileAdapter.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
