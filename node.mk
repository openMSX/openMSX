# $Id$

SUBDIRS:= \
	src \
	m4 \
	doc \
	Contrib

DIST:= \
	alternative.mk platform-*.mk flavour-*.mk detectsys.* \
	ChangeLog AUTHORS GPL README TODO \
	share

# Backwards compatibility for auto* system:
DIST+= \
	COPYING INSTALL NEWS \
	config-i686.sh config-mingw32.sh \
	configure configure.ac \
	openmsx.lsm.in openmsx.spec.in \
	Makefile.am \
	acinclude.m4 aclocal.m4

$(eval $(PROCESS_NODE))

