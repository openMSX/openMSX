#!/bin/sh

SDL_CONFIG="/home/andete/projects/openMSX/mingw/SDL-1.2.3/i386-mingw32msvc/bin/sdl-config" \
XML_LIBS="/home/andete/projects/openMSX/mingw/libxml2-2.4.13/lib" \
XML_CFLAGS="-I/home/andete/projects/openMSX/mingw/libxml2-2.4.13/include" \
CC=/usr/bin/i586-mingw32msvc-gcc \
CXX=/usr/bin/i586-mingw32msvc-c++ \
ac_cv_c_bigendian=no \
ac_cv_sizeof_long=4 \
ac_cv_sizeof_bool=1 \
./configure \
	--target=i586-mingw32msvc \
	--disable-sdltest \
	--disable-xmltest

# mingw/libxml2-2.4.13/lib
