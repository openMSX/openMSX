# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f ../.debug && echo "-DDEBUG"` -I$(srcdir)/.. -I$(srcdir)/../cpu -I$(srcdir)/../config -I$(srcdir)/../thread -I$(srcdir)/../file -I$(srcdir)/../events @XML_CFLAGS@ @SDL_CFLAGS@
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

noinst_LTLIBRARIES        = libSDL_console.la
libSDL_console_la_SOURCES = \
	Console.cc Console.hh \
	ConsoleRenderer.cc ConsoleRenderer.hh \
	OSDConsoleRenderer.cc OSDConsoleRenderer.hh \
	CircularBuffer.cc CircularBuffer.hh \
	SDLConsole.cc SDLConsole.hh \
	GLConsole.cc GLConsole.hh \
	CommandController.cc CommandController.hh \
	Command.cc Command.hh \
	Font.cc Font.hh \
	DummyFont.cc DummyFont.hh \
	SDLFont.cc SDLFont.hh \
	GLFont.cc GLFont.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
