# $Id$
#
# This flavour enables link-time optimization (LTO).
# This requires GCC 4.5 or higher; it's known to work on GCC 4.7.1.

include build/flavour-opt.mk

# Enable LTO for compiling and linking.
# Don't bother with native code output in the compile phase, since new code
# will be generated in the link phase.
# Ideally LTO would be enabled for the 3rdparty libs as well, but I haven't
# been able to make that work.
COMPILE_FLAGS+=-flto -fno-fat-lto-objects
LINK_FLAGS+=-flto
