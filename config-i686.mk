# $Id$
#
# Configuration for "i686" flavour:
# Optimised for Pentium 2 and higher.

# Optimisation flags.
CXXFLAGS+=-O3 -mcpu=pentiumpro -march=pentiumpro -ffast-math -funroll-loops

# Strip executable.
LDFLAGS+=--strip-all
