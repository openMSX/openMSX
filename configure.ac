dnl $Id$
dnl Process this file with autoconf to produce a configure script.


AC_INIT(src/main.cc)

ISODATE=`date +%Y-%m-%d`
OPENMSX_VERSION=0.3.3[CVS]

dnl Change default prefix.
AC_PREFIX_DEFAULT(/opt/openMSX)

dnl Put datadir in config.h.
dnl This location will be compiled into the executable, because there doesn't
dnl seem to be an easy and reliable way to detect this at run time.
AC_DEFINE_DIR(DATADIR, datadir, [Directory containing platform independent data files.])

dnl create a config.h file (Automake will add -DHAVE_CONFIG_H)
AM_CONFIG_HEADER(src/config.h)

AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE(openmsx, $OPENMSX_VERSION)

AC_SUBST(VERSION)
AC_SUBST(ISODATE)

dnl libtool
AC_PROG_LIBTOOL

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

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
AC_CHECK_LIB(SDL_image, IMG_Load, LIB="$LIBS -lSDL_image", AC_MSG_ERROR([Sorry no SDL_image library found]))

dnl libpng
AC_CHECK_HEADERS(png.h, break)
AC_CHECK_LIB(png, png_create_read_struct, LIB="$LIBS -lpng")

dnl check if fstream can be used as template
AC_CXX_FSTREAM_TEMPL

AM_CONDITIONAL(PROFILE, test "$enable_profile" = yes)
AM_CONDITIONAL(RELEASE, test "$enable_release" = yes)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h strings.h string.h)
AC_CHECK_HEADERS(sys/mman.h, [
  openmsx_have_sys_mman_h=yes
],[
  openmsx_have_sys_mman_h=no
]
)
AC_CHECK_HEADERS(sys/socket.h, [
  openmsx_have_sys_socket_h=yes
],[
  openmsx_have_sys_socket_h=no
]
)
AC_CHECK_HEADERS(linux/rtc.h, [
  openmsx_have_linux_rtc_h=yes
],[
  openmsx_have_linu_rtc_h=no
]
)

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.
AC_CHECK_FUNCS(strtoll atoll)
AC_CHECK_FUNCS(ftruncate)

dnl Check for mmap.
AC_FUNC_MMAP

dnl With/Without X11
AC_PATH_X
AC_PATH_XTRA
if test x"$have_x" = "xyes"; then
	with_x="yes"
	no_x="no"
	X_CXXFLAGS="$X_CFLAGS"
	X_LIBS="$X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
else
	with_x="no"
	no_x="yes"
	AC_DEFINE(X_DISPLAY_MISSING, 1, [X11 is not available])
	X_CFLAGS=''
	X_CXXFLAGS=''
	X_LIBS=''
fi
AC_SUBST(X_CFLAGS)
AC_SUBST(X_CXXFLAGS)
AC_SUBST(X_LIBS)



dnl Do final output.
AC_OUTPUT(Makefile
openmsx.lsm
openmsx.spec
doc/Makefile
doc/manual/Makefile
m4/Makefile
src/Makefile
src/cassette/Makefile
src/commands/Makefile
src/config/Makefile
src/console/Makefile
src/cpu/Makefile
src/debugger/Makefile
src/events/Makefile
src/input/Makefile
src/fdc/Makefile
src/file/Makefile
src/ide/Makefile
src/libxmlx/Makefile
src/memory/Makefile
src/serial/Makefile
src/settings/Makefile
src/sound/Makefile
src/thread/Makefile
src/video/Makefile
)
