#!/bin/sh

CXXFLAGS="-O3 -mcpu=pentiumpro -march=pentiumpro -ffast-math -funroll-loops -I/usr/include/libxml2/libxml"

echo Configuring for compile with: ${CXXFLAGS}
rm -f config.cache
CXXFLAGS=${CXXFLAGS} ./configure $*
