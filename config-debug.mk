# $Id$
#
# Configuration for "debug" flavour:
# Build with all debugging info, no optimisations.

# Debug flags.
CXXFLAGS+=-O0 -g -DDEBUG

# Strip executable?
OPENMSX_STRIP:=false
