#!/bin/sh
make \
	OPENMSX_TARGET_OS=darwin-app OPENMSX_TARGET_CPU=ppc \
	OPENMSX_FLAVOUR=bindist \
	PATH=$PWD/derived/3rdparty/install/bin:$PATH \
	bindist
