# Configuration for "ppc" flavour:
# Optimised for PPC-G4 and higher.

# Start with generic optimisation flags.
include src_build/flavour-opt.mk

# Add PPC specific flags.
CXXFLAGS+=-mcpu=G4 -mtune=G4 -mpowerpc-gfxopt -maltivec -mabi=altivec
