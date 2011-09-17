# $Id$
#
# This flavour enables link-time optimization (LTO).
# This requires GCC 4.5 or higher.

# Optimization flags for preprocessing.
CXXFLAGS+=-DNDEBUG
# Optimization flags for compilation and linking.
OPT_FLAGS+=-O3 -ffast-math -funroll-loops

# LTO must be enabled for both compiling and linking, on both the 3rdparty
# libs and openMSX itself.
# Optimization flags should be passed to both the compile and link step.
TARGET_FLAGS+=-flto $(OPT_FLAGS)
