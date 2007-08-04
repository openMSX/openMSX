# $Id$
#
# Configuration for x86 CPUs.

# Big or little endian?
BIG_ENDIAN:=false

# Allow unaligned memory accesses
UNALIGNED_MEMORY_ACCESS:=true

# Default build flavour.
ifeq ($(filter darwin%,$(OPENMSX_TARGET_OS)),)
# To run openMSX with decent speed, at least a Pentium 2 class machine
# is needed, so let's optimise for that.
OPENMSX_FLAVOUR?=i686
else
# The system headers of OS X use SSE features, which are not available on
# i686, so we only use the generic optimisation flags instead.
OPENMSX_FLAVOUR?=opt
endif
