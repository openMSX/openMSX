# Generic optimisation flavour:
# does not target any specific CPU.

# Optimisation flags.
CXXFLAGS+=-O3 -DNDEBUG -ffast-math

# openMSX crashes when the win32 version is built with -fomit-frame-pointer
# TODO: investigate and recheck when new compiler is used.
ifneq ($(OPENMSX_TARGET_OS),mingw32)
CXXFLAGS+=-fomit-frame-pointer
endif

# Strip executable?
OPENMSX_STRIP:=true
