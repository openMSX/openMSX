dnl a bunch of enables
dnl (c) 2001 Joost Yervante Damad
dnl License: GPL
dnl $Id$

AC_DEFUN([AC_ENABLE_SCC],[
AC_MSG_CHECKING(if we want SCC)
AC_ARG_ENABLE(SCC, [  --disable-SCC           Disable SCC],enable_scc=no,enable_scc=yes)
if test "$enable_scc" = no
then
  AC_MSG_RESULT(no)
  AC_DEFINE(DONT_WANT_SCC,, [define if SCC is not wanted])
else
  AC_MSG_RESULT(yes)
fi
])

AC_DEFUN([AC_ENABLE_FMPAC],[
AC_MSG_CHECKING(if we want FMPAC)
AC_ARG_ENABLE(FMPAC, [  --disable-FMPAC         Disable FMPAC],enable_fmpac=no,enable_fmpac=yes)
if test "$enable_fmpac" = no
then
    AC_MSG_RESULT(no)
    AC_DEFINE(DONT_WANT_FMPAC,, [define if FMPAC is not wanted])
else
  AC_MSG_RESULT(yes)
fi
])

AC_DEFUN([AC_ENABLE_MSXMUSIC],[
AC_MSG_CHECKING(if we want MSXMUSIC)
AC_ARG_ENABLE(MSXMUSIC, [  --disable-MSXMUSIC      Disable MSXMUSIC],enable_msxmusic=no,enable_msxmusic=yes)
if test "$enable_MSXMUSIC" = no
then
  AC_MSG_RESULT(no)
  AC_DEFINE(DONT_WANT_MSXMUSIC,, [define if MSXMUSIC is not wanted])
else
  AC_MSG_RESULT(yes)
fi
])
