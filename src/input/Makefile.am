# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f ../.debug && echo "-DDEBUG"` \
	-I$(srcdir)/.. -I$(srcdir)/../cpu \
	-I$(srcdir)/../config \
	-I$(srcdir)/../file \
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

noinst_LTLIBRARIES        = libInput.la
libInput_la_SOURCES = \
	Keyboard.hh Keyboard.cc \
	Joystick.hh Joystick.cc \
	KeyJoystick.hh KeyJoystick.cc \
	Mouse.hh Mouse.cc \
	GrabInput.hh GrabInput.cc 
	
nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
