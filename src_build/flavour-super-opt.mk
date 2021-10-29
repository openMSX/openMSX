# This flavour enables some extra optimizations, though these options may
# not work everywhere or may not be benefical in all cases.

# Start with generic optimisation flags.
include src_build/flavour-opt.mk

# Add CPU specific optimization flags:
#  march=native is only supported starting from gcc-4.2.x
#  comment out this line if you're compiling on an older gcc version
CXXFLAGS+=-march=native -mtune=native

# Use computed goto's to speedup Z80 emulation:
# - Computed goto's are a gcc extension, it's not part of the official c++
#   standard. So this will only work if you use gcc as your compiler (it
#   won't work with visual c++ for example)
# - This is only beneficial on CPUs with branch prediction for indirect jumps
#   and a reasonable amout of cache. For example it is very benefical for a
#   intel core2 cpu (10% faster), but not for a ARM920 (a few percent slower)
# - Compiling src/cpu/CPUCore.cc with computed goto's enabled is very demanding
#   on the compiler. On older gcc versions it requires upto 1.5GB of memory.
#   But even on more recent gcc versions it still requires around 700MB.
CXXFLAGS+=-DUSE_COMPUTED_GOTO
