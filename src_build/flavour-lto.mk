# This flavour enables link-time optimization (LTO).
# This requires GCC 4.5 or higher; it's known to work on GCC 4.6.3.

include src_build/flavour-opt.mk

# Enable LTO for compiling and linking.
# Don't bother with native code output in the compile phase, since new code
# will be generated in the link phase.
# Ideally LTO would be enabled for the 3rdparty libs as well, but I haven't
# been able to make that work.
COMPILE_FLAGS+=-flto
LINK_FLAGS+=-flto=jobserver

# Enable this line to speed up compilation. This is supported from gcc-4.7
# onward. Quote from the gcc manual:
#     -fno-fat-lto-objects improves compilation time over plain LTO, but
#     requires the complete toolchain to be aware of LTO. It requires a
#     linker with linker plugin support for basic functionality.
#COMPILE_FLAGS+=-fno-fat-lto-objects
