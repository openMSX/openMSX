# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. \
	-I$(srcdir)/../config \
	-I$(srcdir)/../console \
	-I$(srcdir)/../cpu \
	-I$(srcdir)/../events \
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

noinst_LTLIBRARIES = libCommands.la
libCommands_la_SOURCES = \
	Command.hh \
	CommandController.cc CommandController.hh \
	InfoCommand.cc InfoCommand.hh \
	InfoTopic.hh \
	AliasCommands.cc AliasCommands.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
