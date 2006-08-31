# $Id$
#
# ARM specific optimization flags

# Optimisation flags:

# overrule optimization level for arm
# with -finline-functions gcc (4.1.2 20060814 (prerelease) (Debian 4.1.1-11)) generates bad code
CXXFLAGS+=-O2 -funswitch-loops -fgcse-after-reload -ffast-math -funroll-loops -DNDEBUG

# Strip executable?
OPENMSX_STRIP:=true
