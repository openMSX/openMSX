# Configuration for Pandora.

ifeq ($(OPENMSX_TARGET_CPU),arm)
# Note: CXX is automatically set by the "setprj" command.
# Target Cortex A8 CPU.
TARGET_FLAGS+=-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
endif

# Pandora is a Linux system.
include build/platform-linux.mk

# Identify the binary executable file.
EXEEXT:=.bin
