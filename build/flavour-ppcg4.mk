# $Id$
#
# Configuration for "ppc" flavour:
# Optimised for PPC-G4 and higher.

# Optimisation flags.
CXXFLAGS+=-O3 -DNDEBUG -mpowerpc-gfxopt
#CXXFLAGS+=-mcpu=7450 -maltivec -mabi=altivec
CXXFLAGS+=-mcpu=G4 -maltivec -mabi=altivec

# Strip executable?
# TODO: Stripping would be good, but linker doesn't understand "--strip-all".
OPENMSX_STRIP:=false

