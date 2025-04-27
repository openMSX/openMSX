# ARM specific optimization flags

# Optimisation flags:

# overrule optimization level for m68k
# TODO: is this still strictly needed?
CXXFLAGS+=-O1 -DNDEBUG


# Strip executable?
OPENMSX_STRIP:=true
