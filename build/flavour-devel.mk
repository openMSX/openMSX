# $Id$
#
# Configuration for "devel" flavour:
# Build with debug symbols, no debug prints, some optimisations.

# Debug flags.
CXXFLAGS+=-O2 -g

# Strip executable?
OPENMSX_STRIP:=false
