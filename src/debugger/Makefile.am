# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../thread \
	-I$(srcdir)/../memory \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../events \
	-I$(srcdir)/../console \
	-I$(srcdir)/../config \
	-I$(srcdir)/../file \
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

noinst_LTLIBRARIES = libDebugger.la
libDebugger_la_SOURCES = \
	DebugView.cc DebugView.hh \
	MemoryView.cc MemoryView.hh \
	DumpView.cc DumpView.hh \
	DisAsmView.cc DisAsmView.hh \
	DebugConsole.cc DebugConsole.hh \
	ViewControl.cc ViewControl.hh \
	DebugInterface.cc DebugInterface.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
