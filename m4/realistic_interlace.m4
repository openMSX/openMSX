dnl realistic interlace macro
dnl (c) 2001 Joost Yervante Damad
dnl License: GPL
dnl $Id$

AC_DEFUN([AC_REALISTIC_INTERLACE],[
AC_MSG_CHECKING(if we want realistic interlacing)
AC_ARG_ENABLE(interlacing, [  --enable-interlacing    Enable realistic interlacing],enable_interlacing=yes,enable_interlacing=no)

if test "$enable_interlacing" = yes
then
  AC_MSG_RESULT(yes)
  AC_DEFINE(WANT_REALISTIC_INTERLACE,, [define if realistic interlace is wanted])
else
  AC_MSG_RESULT(no)
fi

])


