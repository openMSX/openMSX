# $Id$
#
# Configuration for "ppc64" flavour:

# Optimisation flags.
CXXFLAGS+=-O3 -DNDEBUG -mpowerpc-gfxopt
#CXXFLAGS+=-mcpu=750
#CXXFLAGS+=-mcpu=G3

# Strip executable?
# TODO: Stripping would be good, but linker doesn't understand "--strip-all".
OPENMSX_STRIP:=false

