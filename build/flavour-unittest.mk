# Configuration for "unittest" flavour:
# Build executable that runs unit tests.

# Debug flags.
CXXFLAGS+=-O3 -g -DUNITTEST -IContrib/catch2 -fsanitize=address

# Strip executable?
OPENMSX_STRIP:=false

UNITTEST:=true
