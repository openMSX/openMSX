# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f ../.debug && echo "-DDEBUG"` -fhuge-objects -Icpu @XML_CFLAGS@ @SDL_CFLAGS@ @GL_CFLAGS@
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
	SDLConsole.cc SDLConsole.hh \
	ConsoleCommand.hh \
	SDLFont.cc SDLFont.hh

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f ../.debug
setdebug:
	@touch ../.debug
