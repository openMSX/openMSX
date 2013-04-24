# Configuration for "i686" flavour:
# Optimised for Pentium 3, but compatible with Pentium 2 and higher.

# Start with generic optimisation flags.
include build/flavour-opt.mk

# Add x86 specific flags.
CXXFLAGS+=-march=i686 -mtune=pentium3
