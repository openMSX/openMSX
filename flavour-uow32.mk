# $Id$
#
# Configuration for UOW32 flavour:
# Optimised for Pentium 3 but runnable at i386 and higher.

# Optimisation flags.
CXXFLAGS+= \
	-mcpu=pentium3 -O \
	-fno-force-mem -fno-force-addr \
	-fstrength-reduce -fexpensive-optimizations -fschedule-insns2 \
	-fomit-frame-pointer \
	-DNDEBUG -DUOW32

# Strip executable?
OPENMSX_STRIP:=true
