# "opt", but for the original x86-64 baseline (SSE2) instead of x86-64-v3.
# Use this to build a binary that still runs on pre-Haswell / pre-Excavator
# CPUs. On non-x86-64 targets this flavour is the same as "opt".

include build/flavour-opt.mk

ifeq ($(OPENMSX_TARGET_CPU),x86_64)
# Overrides the default -march=x86-64-v3 added by main.mk. GCC calls this
# baseline -march=x86-64; there is no -march=x86-64-v1.
CXXFLAGS+=-march=x86-64
endif
