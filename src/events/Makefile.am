# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../commands \
	-I$(srcdir)/../config \
	-I$(srcdir)/../console \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../debugger \
	-I$(srcdir)/../file \
	-I$(srcdir)/../input \
	-I$(srcdir)/../settings \
	-I$(srcdir)/../thread \
	-I$(srcdir)/../video \
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

noinst_LTLIBRARIES = libEvents.la
libEvents_la_SOURCES = \
	EventDistributor.cc EventDistributor.hh \
	EventListener.hh \
	HotKey.cc HotKey.hh \
	AfterCommand.cc AfterCommand.hh \
	KeyEventInserter.cc KeyEventInserter.hh \
	Keys.cc Keys.hh \
	SDLEventInserter.cc SDLEventInserter.hh \
	CliCommOutput.cc CliCommOutput.hh \
	CliCommInput.cc CliCommInput.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
