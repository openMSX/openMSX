dnl ifstream<byte> macro
dnl (c) 2001 Joost Yervante Damad
dnl License: GPL

AC_DEFUN([AC_CXX_FSTREAM_TEMPL],
[AC_MSG_CHECKING(whether ifstream<byte> works)
 AC_LANG_SAVE
 AC_LANG_CPLUSPLUS
 AC_TRY_COMPILE(
[
#include <fstream>
typedef unsigned char byte;
]
,
[
std::ifstream<byte> hello("WORLD");
return 0;
]
,
ac_cv_cxx_fstream_templ=yes,
ac_cv_cxx_fstream_templ=no
)
 AC_LANG_RESTORE

if test "$ac_cv_cxx_fstream_templ" = yes; then
  AC_DEFINE(HAVE_FSTREAM_TEMPL,,
            [define if the stdc++ library supports ifstream as template])
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

])


