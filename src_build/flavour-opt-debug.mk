# Generic optimisation flavour:
# does not target any specific CPU.

# Optimisation flags.
CXXFLAGS+=-O3 -DNDEBUG -ffast-math -g

# Strip executable?
OPENMSX_STRIP:=false
