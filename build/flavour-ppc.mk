# Configuration for "ppc" flavour:
# Optimised for G3 PPC.

# Start with generic optimisation flags.
include build/flavour-opt.mk

# Add PPC specific flags.
CXXFLAGS+=-mcpu=G3 -mtune=G4
