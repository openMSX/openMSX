dnl $Id$
dnl Process this file with autoconf to produce a configure script.


AC_INIT(src/main.cc)

ISODATE=`date +%Y-%m-%d`
OPENMSX_VERSION=0.3.4[CVS]

dnl Change default prefix.
AC_PREFIX_DEFAULT(/opt/openMSX)

dnl Put datadir in config.h.
dnl This location will be compiled into the executable, because there doesn't
dnl seem to be an easy and reliable way to detect this at run time.
AC_DEFINE_DIR(DATADIR, datadir, [Directory containing platform independent data files.])

dnl create a config.h file
AC_CONFIG_HEADERS(src/config.h)

dnl for install-sh, config.guess, config.sub, and so on,
dnl we still need automake unfortunately.
dnl even though we don't use Makefile.am anymore... so bad!
AM_INIT_AUTOMAKE(openmsx, $OPENMSX_VERSION)

AC_SUBST(VERSION)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

dnl check ENDIAN-ness
AC_C_BIGENDIAN

dnl Checks for header files.
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
  openmsx_have_linux_rtc_h=no
]
)

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.
AC_CHECK_FUNCS(ftruncate)
AC_CHECK_FUNCS(gettimeofday)
AC_CHECK_FUNCS(usleep)

dnl Check for mmap.
AC_FUNC_MMAP


dnl Checks for library headers.

dnl openGL
AC_CHECK_HEADERS(gl.h GL/gl.h, break)

dnl SDL_image
AC_CHECK_HEADERS(SDL_image.h SDL/SDL_image.h , break)

dnl libtcl
AC_CHECK_HEADERS(tcl.h tcl8.4/tcl.h tcl8.3/tcl.h , break)

dnl With/Without X11
AC_PATH_X
AC_PATH_XTRA
if test x"$have_x" = "xyes"; then
	with_x="yes"
	no_x="no"
else
	with_x="no"
	no_x="yes"
	AC_DEFINE(X_DISPLAY_MISSING, 1, [X11 is not available])
fi



dnl Do final output. the Makefile is a dummy to prevent automake's blame
AC_OUTPUT(Makefile)
