# $Id$

noinst_LTLIBRARIES = libxmlx.la
libxmlx_la_SOURCES = libxmlx.cc xmlx.hh

AM_CXXFLAGS_BASE = -Wall -I$(srcdir)/.. @XML_CFLAGS@
#AM_CXXFLAGS = -Wall -DXMLX_DEBUG -g3 -pipe
#AM_CXXFLAGS = -Wall -g3 -pipe -I$(srcdir)/.. @XML_CFLAGS@
if RELEASE
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE)
else
AM_CXXFLAGS = $(AM_CXXFLAGS_BASE) -g3
endif

#  uncomment the following if libxmlx requires the math library
#libxmlx_LDADD=-lm

# we do not want all the libs that openmsx links at the moment
#LIBS=@XML_LIBS@
#libxmlx_la_LIBADD=@XML_LIBS@

# GCC 2.95 needs to link against libstdc++
libxmlx_la_LIBADD = -lstdc++

#bin_PROGRAMS  = xmlxdump
noinst_PROGRAMS  = xmlxdump
xmlxdump_SOURCES = xmlxdump.cc
xmlxdump_LDADD   = libxmlx.la @XML_LIBS@

#EXTRA_DIST=libxmlx.lsm.in #NAME.texinfo libxmlx.spec.in

#  if you write a self-test script named `chk', uncomment the
#  following and add `chk' to the EXTRA_DIST list
#TESTS=chk

#  build and install the .info pages
#info_TEXINFOS = libxmlx.texinfo
#libxmlx_TEXINFOS = gpl.texinfo

#  install the man pages
#man_MANS=libxmlx.1
