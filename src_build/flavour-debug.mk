# Configuration for "debug" flavour:
# Build with all debugging info, no optimisations.

# Debug flags.
CXXFLAGS+=-O0 -g -DDEBUG -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC \
          -D_GLIBCPP_CONCEPT_CHECKS

# Strip executable?
OPENMSX_STRIP:=false
