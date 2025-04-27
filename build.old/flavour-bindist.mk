# Generic flavour for binary distributions.
# It disables debugging and aims for a small binary.

# Optimisation flags.
CXXFLAGS+=-Os -DNDEBUG

# Strip executable?
OPENMSX_STRIP:=true
