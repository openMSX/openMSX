# $Id$

AM_CXXFLAGS_BASE = -Wall -pipe `test -f $(srcdir)/../.debug && echo "-DDEBUG"` -fhuge-objects -I$(srcdir)/.. -I$(srcdir)/../cpu -I$(srcdir)/../config @XML_CFLAGS@ @SDL_CFLAGS@
if PROFILE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -pg
else
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif
endif

noinst_LTLIBRARIES = libCPU.la
libCPU_la_SOURCES = \
	CPUInterface.hh CPUInterface.cc \
	CPU.hh CPU.cc CPU.ii \
	CPUCore.n1 CPUCore.n2 CPUTables.nn \
	Z80.cc Z80.hh Z80Tables.nn Z80Dasm.h \
	MSXCPU.hh MSXCPU.cc \
	MSXCPUInterface.hh MSXCPUInterface.cc

nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f $(srcdir)/../.debug
setdebug:
	@touch $(srcdir)/../.debug
