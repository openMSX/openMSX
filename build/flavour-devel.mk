# Configuration for "devel" flavour:
# Build with debug symbols, no debug prints, some optimisations.

# Debug flags.
CXXFLAGS+=-O2 -g
# Extra warnings, only works with recent gcc versions
#CXXFLAGS+=-ansi -pedantic -Wno-long-long -Wextra -Wno-missing-field-initializers

# Strip executable?
OPENMSX_STRIP:=false
