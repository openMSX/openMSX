# $Id$

# we want most warnings

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` -I$(srcdir)/../cpu -I$(srcdir)/../sound -I$(srcdir)/../config @XML_CFLAGS@ @SDL_CFLAGS@
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

# SUBDIRS = 

bin_PROGRAMS=\
	bin2c

bin2c_SOURCES=\
	bin2c.cc

MAINTAINERCLEANFILES = Makefile.in config.h.in stamp-h.in

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
