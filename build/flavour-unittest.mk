# Configuration for "unittest" flavour:
# Build executable that runs unit tests.

# Debug flags.
CXXFLAGS+=-O3 -g -fsanitize=address

# Flags that are needed to build the unit tests at all: COMPILE_FLAGS, so that
# overriding CXXFLAGS from the command line doesn't remove them.
COMPILE_FLAGS+=-DUNITTEST -IContrib/catch2

# Strip executable?
OPENMSX_STRIP:=false

UNITTEST:=true
