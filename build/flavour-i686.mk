# $Id$
#
# Configuration for "i686" flavour:
# Optimised for Pentium 2 and higher.

# Optimisation flags.
CXXFLAGS+=-O3 -DNDEBUG \
	-march=i686 -ffast-math -funroll-loops

# Strip executable?
OPENMSX_STRIP:=true
