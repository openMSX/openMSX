dnl ####################### -*- Mode: M4 -*- ###########################
dnl Copyright (C) 98, 1999 Matthew D. Langston <langston@SLAC.Stanford.EDU>
dnl
dnl This file is free software; you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This file is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this file; if not, write to:
dnl
dnl   Free Software Foundation, Inc.
dnl   Suite 330
dnl   59 Temple Place
dnl   Boston, MA 02111-1307, USA.
dnl ####################################################################


dnl NOTE: The AC_HELP_STRING macro has been accepted for inclusion in
dnl       Autoconf 2.15, but this version of Autoconf hasn't been
dnl       released yet as of the time that I write this.  Therefore, I
dnl       am including it here for the time being


dnl AC_HELP_STRING
dnl --------------
dnl
dnl usage: AC_HELP_STRING(LHS, RHS, HELP-STRING)
dnl
dnl Format an Autoconf macro's help string so that it looks pretty when
dnl the user executes "configure --help".  This macro take three
dnl arguments, a "left hand side" (LHS), a "right hand side" (RHS), and
dnl a variable (HELP-STRING) to set to the pretty-printed concatenation
dnl of LHS and RHS (the new, pretty-printed "help string").
dnl
dnl The resulting string in HELP-STRING is suitable for use in other
dnl macros that require a help string (e.g. AC_ARG_WITH).
dnl 
dnl AC_DEFUN(AC_HELP_STRING,
pushdef([AC_HELP_STRING],
[
dnl 
dnl Here is the sample string from the Autoconf manual (Node: External
dnl Software) which shows the proper spacing for help strings.
dnl 
dnl    --with-readline         support fancy command line editing
dnl  ^ ^                       ^ 
dnl  | |                       |
dnl  | column 2                column 26     
dnl  |
dnl  column 0
dnl 
dnl A help string is made up of a "left hand side" (LHS) and a "right
dnl hand side" (RHS).  In the example above, the LHS is
dnl "--with-readline", while the RHS is "support fancy command line
dnl editing".
dnl 
dnl If the LHS extends past column 24, then the LHS is terminated with a
dnl newline so that the RHS is on a line of its own beginning in column
dnl 26.
dnl 
dnl Therefore, if the LHS were instead "--with-readline-blah-blah-blah",
dnl then the AC_HELP_STRING macro would expand into:
dnl
dnl
dnl    --with-readline-blah-blah-blah
dnl  ^ ^                       support fancy command line editing
dnl  | |                       ^ 
dnl  | column 2                |
dnl  column 0                  column 26     

dnl We divert everything to AC_DIVERSION_NOTICE (which gets output very
dnl early in the configure script) because we want the user's help
dnl string to be set before it is used.

AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)dnl
# This is from AC_HELP_STRING
lhs="$1"
rhs="$2"

lhs_column=25
rhs_column=`expr $lhs_column + 1`

# Insure that the LHS begins with exactly two spaces.
changequote(, )dnl
lhs=`echo "$lhs" | sed -n -e "s/[ ]*\(.*\)/  \1/p"`
changequote([, ])dnl

# Is the length of the LHS less than $lhs_column?
if ! `echo "$lhs" | grep ".\{$lhs_column\}" > /dev/null 2>&1`; then

  # Pad the LHS with spaces.  Note that padding the LHS is an
  # "expensive" operation (i.e. expensive in the sense of there being
  # multiple calls to `grep') only the first time AC_HELP_STRING is
  # called.  Once this macro is called once, subsequent calls will be
  # nice and zippy.
  : ${lhs_pad=""}
changequote(, )dnl
  while ! `echo "$lhs_pad" | grep "[ ]\{$lhs_column\}" > /dev/null 2>&1`; do
changequote([, ])dnl
    lhs_pad=" $lhs_pad"
  done

  lhs="${lhs}${lhs_pad}"
changequote(, )dnl
$3=`echo "$lhs" | sed -n -e "/.\{$lhs_column\}[ ][ ]*$/ s/\(.\{$rhs_column\}\).*/\1$rhs/p"`
changequote([, ])dnl

else

  # Build up a string of spaces to pad the left-hand-side of the RHS
  # with.  Note that padding the RHS is an "expensive" operation
  # (i.e. expensive in the sense of there being multiple calls to
  # `grep') only the first time AC_HELP_STRING is called.  Once this
  # macro is called once, subsequent calls will be nice and zippy.
  : ${rhs_pad=""}
changequote(, )dnl
  while ! `echo "$rhs_pad" | grep "[ ]\{$rhs_column\}" > /dev/null 2>&1`; do
changequote([, ])dnl
    rhs_pad=" $rhs_pad"
  done

  # Strip all leading spaces from the RHS.
changequote(, )dnl
  rhs=`echo "$rhs" | sed -n -e "s/[ ]*\(.*\)/\1/p"`
changequote([, ])dnl

$3="$lhs
${rhs_pad}${rhs}"
fi 
AC_DIVERT_POP()dnl
])
