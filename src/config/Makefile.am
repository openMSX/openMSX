# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f ../.debug && echo "-DDEBUG"` -fhuge-objects -Icpu @XML_CFLAGS@ @SDL_CFLAGS@
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

noinst_LTLIBRARIES = libConfig.la
libConfig_la_SOURCES = \
	MSXConfig.cc \
	MSXConfig.hh \
	XMLConfig.cc \
	XMLConfig.hh \
	MSXFilePath.hh \
	MSXFilePath.cc \
	XMLFilePath.hh \
	XMLFilePath.cc


nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f ../.debug
setdebug:
	@touch ../.debug
