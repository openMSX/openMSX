dnl $Id$
dnl Process this file with autoconf to produce a configure script.


AC_INIT(src/main.cc)
dnl Every other copy of the package version number gets its value from here
AM_INIT_AUTOMAKE(openmsx, 0.2.1)

if test "$prefix" = "NONE"; then
  prefix="/opt/openmsx"
fi
LIBS="-L$prefix/lib $LIBS"

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(src/config.h)

AC_SUBST(VERSION)

ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

AC_CANONICAL_HOST

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CXX

dnl libtool
AC_PROG_LIBTOOL

dnl check ENDIAN-ness
AC_C_BIGENDIAN

dnl check size of long
AC_CHECK_SIZEOF(long)

dnl bool is a c++ type, soo we need to do this test in c++:
AC_LANG_CPLUSPLUS
AC_CHECK_SIZEOF(bool)
AC_LANG_C

dnl Checks for libraries.

dnl libSDL
AM_PATH_SDL
dnl CXXFLAGS="$CXXFLAGS $SDL_CFLAGS"
dnl LIBS="$SDL_LIBS $LIBS"

dnl libxml
AM_PATH_XML2
dnl CXXFLAGS="$CXXFLAGS $XML_CFLAGS"
dnl LIBS="$XML_LIBS $LIBS"

AC_ENABLE_GLRENDERER
if test "$enable_GLRENDERER" = "yes"; then
  dnl openGL
  AC_CHECK_HEADERS(gl.h GL/gl.h, break)
  AC_CHECK_LIB(GL, glViewport)
fi

dnl SDL_image
AC_CHECK_HEADERS(SDL_image.h SDL/SDL_image.h , break)
AC_CHECK_LIB(SDL_image, IMG_Load, break, AC_MSG_ERROR([Sorry no SDL_image library found]))

dnl libpng
AC_CHECK_HEADERS(png.h, break)
AC_CHECK_LIB(png, png_create_read_struct)

dnl check if fstream can be used as template
AC_CXX_FSTREAM_TEMPL

AM_CONDITIONAL(PROFILE, test "$enable_profile" = yes)
AM_CONDITIONAL(RELEASE, test "$enable_release" = yes)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(string.h)
AC_CHECK_HEADERS(strings.h)

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.
AC_CHECK_FUNCS(strtoll atoll)

AC_OUTPUT(Makefile openmsx.lsm openmsx.spec doc/Makefile src/Makefile m4/Makefile src/cfg/Makefile src/libxmlx/Makefile src/ConsoleSource/Makefile src/cpu/Makefile src/sound/Makefile src/config/Makefile src/fdc/Makefile src/file/Makefile src/ide/Makefile src/thread/Makefile src/cassette/Makefile src/events/Makefile src/memory/Makefile)
