# $Id$
#
# Generic optimisation flavour:
# does not target any specific CPU.

# Optimisation flags.
CXXFLAGS+=-O3 -DNDEBUG \
	-ffast-math -funroll-loops -fomit-frame-pointer

# Strip executable?
OPENMSX_STRIP:=true
