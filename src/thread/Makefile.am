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

noinst_LTLIBRARIES = libThread.la
libThread_la_SOURCES = \
	Thread.cc Thread.hh \
	Mutex.cc Mutex.hh \
	CondVar.cc ComdVar.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
