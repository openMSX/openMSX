# Wrapper that calls Ninja where the build system expects a Makefile.

.PHONY: all install

all:
	ninja

install:
	ninja install
