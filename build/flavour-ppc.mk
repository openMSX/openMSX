# $Id$
#
# Configuration for "ppc" flavour:
# Optimised for G3 PPC.

# Optimisation flags.
CXXFLAGS+=-O3 -DNDEBUG -mpowerpc-gfxopt
#CXXFLAGS+=-mcpu=750
CXXFLAGS+=-mcpu=G3

# TODO: this file is currently made for MacOSX and won't work on debian/PPC
#       lets discuss this on IRC, perhaps they should be detected as separate systems somehow?

# Strip executable?
# TODO: Stripping would be good, but linker doesn't understand "--strip-all".
OPENMSX_STRIP:=false

