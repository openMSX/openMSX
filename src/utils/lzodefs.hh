// $Id$

/* lzodefs.hh -- architecture, OS and compiler specific defines

   This file is part of the LZO real-time data compression library.

   Copyright (C) 2008 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2007 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2006 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2005 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2004 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2003 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2002 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2001 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2000 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1999 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1998 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1997 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1996 Markus Franz Xaver Johannes Oberhumer
   All Rights Reserved.

   The LZO library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The LZO library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the LZO library; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   Markus F.X.J. Oberhumer
   <markus@oberhumer.com>
   http://www.oberhumer.com/opensource/lzo/
 */


#ifndef LZODEFS_HH
#define LZODEFS_HH

#if defined(__CYGWIN32__) && !defined(__CYGWIN__)
#  define __CYGWIN__ __CYGWIN32__
#endif
#if defined(__IBMCPP__) && !defined(__IBMC__)
#  define __IBMC__ __IBMCPP__
#endif
#if defined(__ICL) && defined(_WIN32) && !defined(__INTEL_COMPILER)
#  define __INTEL_COMPILER __ICL
#endif
#if defined(__INTERIX) && defined(__GNUC__) && !defined(_ALL_SOURCE)
#  define _ALL_SOURCE 1
#endif
#if defined(__mips__) && defined(__R5900__)
#  if !defined(__LONG_MAX__)
#    define __LONG_MAX__ 9223372036854775807L
#  endif
#endif
#define LZO_0xffffL             65535ul
#define LZO_0xffffffffL         4294967295ul
#if (LZO_0xffffL == LZO_0xffffffffL)
#  error "your preprocessor is broken 1"
#endif
#if (16ul * 16384ul != 262144ul)
#  error "your preprocessor is broken 2"
#endif
#if defined(__WATCOMC__) && (__WATCOMC__ < 900)
#  define LZO_BROKEN_INTEGRAL_CONSTANTS 1
#endif
#if defined(_CRAY) && defined(_CRAY1)
#  define LZO_BROKEN_SIGNED_RIGHT_SHIFT 1
#endif
#define LZO_PP_STRINGIZE(x)             #x
#define LZO_PP_MACRO_EXPAND(x)          LZO_PP_STRINGIZE(x)
#define LZO_PP_CONCAT2(a,b)             a ## b
#define LZO_PP_CONCAT3(a,b,c)           a ## b ## c
#define LZO_PP_CONCAT4(a,b,c,d)         a ## b ## c ## d
#define LZO_PP_CONCAT5(a,b,c,d,e)       a ## b ## c ## d ## e
#define LZO_PP_ECONCAT2(a,b)            LZO_PP_CONCAT2(a,b)
#define LZO_PP_ECONCAT3(a,b,c)          LZO_PP_CONCAT3(a,b,c)
#define LZO_PP_ECONCAT4(a,b,c,d)        LZO_PP_CONCAT4(a,b,c,d)
#define LZO_PP_ECONCAT5(a,b,c,d,e)      LZO_PP_CONCAT5(a,b,c,d,e)
#define LZO_CPP_STRINGIZE(x)            #x
#define LZO_CPP_MACRO_EXPAND(x)         LZO_CPP_STRINGIZE(x)
#define LZO_CPP_CONCAT2(a,b)            a ## b
#define LZO_CPP_CONCAT3(a,b,c)          a ## b ## c
#define LZO_CPP_CONCAT4(a,b,c,d)        a ## b ## c ## d
#define LZO_CPP_CONCAT5(a,b,c,d,e)      a ## b ## c ## d ## e
#define LZO_CPP_ECONCAT2(a,b)           LZO_CPP_CONCAT2(a,b)
#define LZO_CPP_ECONCAT3(a,b,c)         LZO_CPP_CONCAT3(a,b,c)
#define LZO_CPP_ECONCAT4(a,b,c,d)       LZO_CPP_CONCAT4(a,b,c,d)
#define LZO_CPP_ECONCAT5(a,b,c,d,e)     LZO_CPP_CONCAT5(a,b,c,d,e)
#define __LZO_MASK_GEN(o,b)     (((((o) << ((b)-1)) - (o)) << 1) + (o))
#if defined(__cplusplus)
#  if !defined(__STDC_CONSTANT_MACROS)
#    define __STDC_CONSTANT_MACROS 1
#  endif
#  if !defined(__STDC_LIMIT_MACROS)
#    define __STDC_LIMIT_MACROS 1
#  endif
#endif
#if !defined(__LZO_OS_OVERRIDE)
#if defined(LZO_OS_FREESTANDING)
#  define LZO_INFO_OS           "freestanding"
#elif defined(LZO_OS_EMBEDDED)
#  define LZO_INFO_OS           "embedded"
#elif defined(__IAR_SYSTEMS_ICC__)
#  define LZO_OS_EMBEDDED       1
#  define LZO_INFO_OS           "embedded"
#elif defined(__CYGWIN__) && defined(__GNUC__)
#  define LZO_OS_CYGWIN         1
#  define LZO_INFO_OS           "cygwin"
#elif defined(__EMX__) && defined(__GNUC__)
#  define LZO_OS_EMX            1
#  define LZO_INFO_OS           "emx"
#elif defined(__BEOS__)
#  define LZO_OS_BEOS           1
#  define LZO_INFO_OS           "beos"
#elif defined(__Lynx__)
#  define LZO_OS_LYNXOS         1
#  define LZO_INFO_OS           "lynxos"
#elif defined(__OS400__)
#  define LZO_OS_OS400          1
#  define LZO_INFO_OS           "os400"
#elif defined(__QNX__)
#  define LZO_OS_QNX            1
#  define LZO_INFO_OS           "qnx"
#elif defined(__BORLANDC__) && defined(__DPMI32__) && (__BORLANDC__ >= 0x0460)
#  define LZO_OS_DOS32          1
#  define LZO_INFO_OS           "dos32"
#elif defined(__ZTC__) && defined(DOS386)
#  define LZO_OS_DOS32          1
#  define LZO_INFO_OS           "dos32"
#elif defined(__OS2__) || defined(__OS2V2__)
#    define LZO_OS_OS2          1
#    define LZO_INFO_OS         "os2"
#elif defined(__WIN64__) || defined(_WIN64) || defined(WIN64)
#  define LZO_OS_WIN64          1
#  define LZO_INFO_OS           "win64"
#elif defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WINDOWS_386__)
#  define LZO_OS_WIN32          1
#  define LZO_INFO_OS           "win32"
#elif defined(__MWERKS__) && defined(__INTEL__)
#  define LZO_OS_WIN32          1
#  define LZO_INFO_OS           "win32"
#elif defined(__WINDOWS__) || defined(_WINDOWS) || defined(_Windows)
#    define LZO_OS_WIN32        1
#    define LZO_INFO_OS         "win32"
#elif defined(__DOS__) || defined(__MSDOS__) || defined(_MSDOS) || defined(MSDOS) || (defined(__PACIFIC__) && defined(DOS))
#    define LZO_OS_DOS32        1
#    define LZO_INFO_OS         "dos32"
#elif defined(__WATCOMC__)
#  if defined(__NT__) && (__WATCOMC__ < 1100)
#    define LZO_OS_WIN32        1
#    define LZO_INFO_OS         "win32"
#  elif defined(__linux__) || defined(__LINUX__)
#    define LZO_OS_POSIX        1
#    define LZO_INFO_OS         "posix"
#  else
#    error "please specify a target using the -bt compiler option"
#  endif
#elif defined(__palmos__)
#  define LZO_OS_PALMOS         1
#  define LZO_INFO_OS           "palmos"
#elif defined(__TOS__) || defined(__atarist__)
#  define LZO_OS_TOS            1
#  define LZO_INFO_OS           "tos"
#elif defined(macintosh) && !defined(__ppc__)
#  define LZO_OS_MACCLASSIC     1
#  define LZO_INFO_OS           "macclassic"
#elif defined(__VMS)
#  define LZO_OS_VMS            1
#  define LZO_INFO_OS           "vms"
#elif ((defined(__mips__) && defined(__R5900__)) || defined(__MIPS_PSX2__))
#  define LZO_OS_CONSOLE        1
#  define LZO_OS_CONSOLE_PS2    1
#  define LZO_INFO_OS           "console"
#  define LZO_INFO_OS_CONSOLE   "ps2"
#elif (defined(__mips__) && defined(__psp__))
#  define LZO_OS_CONSOLE        1
#  define LZO_OS_CONSOLE_PSP    1
#  define LZO_INFO_OS           "console"
#  define LZO_INFO_OS_CONSOLE   "psp"
#else
#  define LZO_OS_POSIX          1
#  define LZO_INFO_OS           "posix"
#endif
#if (LZO_OS_POSIX)
#  if defined(_AIX) || defined(__AIX__) || defined(__aix__)
#    define LZO_OS_POSIX_AIX        1
#    define LZO_INFO_OS_POSIX       "aix"
#  elif defined(__FreeBSD__)
#    define LZO_OS_POSIX_FREEBSD    1
#    define LZO_INFO_OS_POSIX       "freebsd"
#  elif defined(__hpux__) || defined(__hpux)
#    define LZO_OS_POSIX_HPUX       1
#    define LZO_INFO_OS_POSIX       "hpux"
#  elif defined(__INTERIX)
#    define LZO_OS_POSIX_INTERIX    1
#    define LZO_INFO_OS_POSIX       "interix"
#  elif defined(__IRIX__) || defined(__irix__)
#    define LZO_OS_POSIX_IRIX       1
#    define LZO_INFO_OS_POSIX       "irix"
#  elif defined(__linux__) || defined(__linux) || defined(__LINUX__)
#    define LZO_OS_POSIX_LINUX      1
#    define LZO_INFO_OS_POSIX       "linux"
#  elif defined(__APPLE__) || defined(__MACOS__)
#    define LZO_OS_POSIX_MACOSX     1
#    define LZO_INFO_OS_POSIX       "macosx"
#  elif defined(__minix__) || defined(__minix)
#    define LZO_OS_POSIX_MINIX      1
#    define LZO_INFO_OS_POSIX       "minix"
#  elif defined(__NetBSD__)
#    define LZO_OS_POSIX_NETBSD     1
#    define LZO_INFO_OS_POSIX       "netbsd"
#  elif defined(__OpenBSD__)
#    define LZO_OS_POSIX_OPENBSD    1
#    define LZO_INFO_OS_POSIX       "openbsd"
#  elif defined(__osf__)
#    define LZO_OS_POSIX_OSF        1
#    define LZO_INFO_OS_POSIX       "osf"
#  elif defined(__solaris__) || defined(__sun)
#    if defined(__SVR4) || defined(__svr4__)
#      define LZO_OS_POSIX_SOLARIS  1
#      define LZO_INFO_OS_POSIX     "solaris"
#    else
#      define LZO_OS_POSIX_SUNOS    1
#      define LZO_INFO_OS_POSIX     "sunos"
#    endif
#  elif defined(__ultrix__) || defined(__ultrix)
#    define LZO_OS_POSIX_ULTRIX     1
#    define LZO_INFO_OS_POSIX       "ultrix"
#  elif defined(_UNICOS)
#    define LZO_OS_POSIX_UNICOS     1
#    define LZO_INFO_OS_POSIX       "unicos"
#  else
#    define LZO_OS_POSIX_UNKNOWN    1
#    define LZO_INFO_OS_POSIX       "unknown"
#  endif
#endif
#endif
#if (LZO_OS_DOS32 || LZO_OS_OS2 || LZO_OS_WIN32 || LZO_OS_WIN64)
#  if (UINT_MAX != LZO_0xffffffffL)
#    error "this should not happen"
#  endif
#  if (ULONG_MAX != LZO_0xffffffffL)
#    error "this should not happen"
#  endif
#endif
#if defined(CIL) && defined(_GNUCC) && defined(__GNUC__)
#  define LZO_CC_CILLY          1
#  define LZO_INFO_CC           "Cilly"
#  if defined(__CILLY__)
#    define LZO_INFO_CCVER      LZO_PP_MACRO_EXPAND(__CILLY__)
#  else
#    define LZO_INFO_CCVER      "unknown"
#  endif
#elif defined(__PATHSCALE__) && defined(__PATHCC_PATCHLEVEL__)
#  define LZO_CC_PATHSCALE      (__PATHCC__ * 0x10000L + __PATHCC_MINOR__ * 0x100 + __PATHCC_PATCHLEVEL__)
#  define LZO_INFO_CC           "Pathscale C"
#  define LZO_INFO_CCVER        __PATHSCALE__
#elif defined(__INTEL_COMPILER)
#  define LZO_CC_INTELC         1
#  define LZO_INFO_CC           "Intel C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__INTEL_COMPILER)
#  if defined(_WIN32) || defined(_WIN64)
#    define LZO_CC_SYNTAX_MSC 1
#  else
#    define LZO_CC_SYNTAX_GNUC 1
#  endif
#elif defined(__POCC__) && defined(_WIN32)
#  define LZO_CC_PELLESC        1
#  define LZO_INFO_CC           "Pelles C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__POCC__)
#elif defined(__llvm__) && defined(__GNUC__) && defined(__VERSION__)
#  if defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#    define LZO_CC_LLVM         (__GNUC__ * 0x10000L + __GNUC_MINOR__ * 0x100 + __GNUC_PATCHLEVEL__)
#  else
#    define LZO_CC_LLVM         (__GNUC__ * 0x10000L + __GNUC_MINOR__ * 0x100)
#  endif
#  define LZO_INFO_CC           "llvm-gcc"
#  define LZO_INFO_CCVER        __VERSION__
#elif defined(__GNUC__) && defined(__VERSION__)
#  if defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#    define LZO_CC_GNUC         (__GNUC__ * 0x10000L + __GNUC_MINOR__ * 0x100 + __GNUC_PATCHLEVEL__)
#  elif defined(__GNUC_MINOR__)
#    define LZO_CC_GNUC         (__GNUC__ * 0x10000L + __GNUC_MINOR__ * 0x100)
#  else
#    define LZO_CC_GNUC         (__GNUC__ * 0x10000L)
#  endif
#  define LZO_INFO_CC           "gcc"
#  define LZO_INFO_CCVER        __VERSION__
#elif defined(__ACK__) && defined(_ACK)
#  define LZO_CC_ACK            1
#  define LZO_INFO_CC           "Amsterdam Compiler Kit C"
#  define LZO_INFO_CCVER        "unknown"
#elif defined(__AZTEC_C__)
#  define LZO_CC_AZTECC         1
#  define LZO_INFO_CC           "Aztec C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__AZTEC_C__)
#elif defined(__BORLANDC__)
#  define LZO_CC_BORLANDC       1
#  define LZO_INFO_CC           "Borland C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__BORLANDC__)
#elif defined(_CRAYC) && defined(_RELEASE)
#  define LZO_CC_CRAYC          1
#  define LZO_INFO_CC           "Cray C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(_RELEASE)
#elif defined(__DMC__) && defined(__SC__)
#  define LZO_CC_DMC            1
#  define LZO_INFO_CC           "Digital Mars C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__DMC__)
#elif defined(__DECC)
#  define LZO_CC_DECC           1
#  define LZO_INFO_CC           "DEC C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__DECC)
#elif defined(__HIGHC__)
#  define LZO_CC_HIGHC          1
#  define LZO_INFO_CC           "MetaWare High C"
#  define LZO_INFO_CCVER        "unknown"
#elif defined(__IAR_SYSTEMS_ICC__)
#  define LZO_CC_IARC           1
#  define LZO_INFO_CC           "IAR C"
#  if defined(__VER__)
#    define LZO_INFO_CCVER      LZO_PP_MACRO_EXPAND(__VER__)
#  else
#    define LZO_INFO_CCVER      "unknown"
#  endif
#elif defined(__IBMC__)
#  define LZO_CC_IBMC           1
#  define LZO_INFO_CC           "IBM C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__IBMC__)
#elif defined(__KEIL__) && defined(__C166__)
#  define LZO_CC_KEILC          1
#  define LZO_INFO_CC           "Keil C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__C166__)
#elif defined(__LCC__) && defined(_WIN32) && defined(__LCCOPTIMLEVEL)
#  define LZO_CC_LCCWIN32       1
#  define LZO_INFO_CC           "lcc-win32"
#  define LZO_INFO_CCVER        "unknown"
#elif defined(__LCC__)
#  define LZO_CC_LCC            1
#  define LZO_INFO_CC           "lcc"
#  if defined(__LCC_VERSION__)
#    define LZO_INFO_CCVER      LZO_PP_MACRO_EXPAND(__LCC_VERSION__)
#  else
#    define LZO_INFO_CCVER      "unknown"
#  endif
#elif defined(_MSC_VER)
#  define LZO_CC_MSC            1
#  define LZO_INFO_CC           "Microsoft C"
#  if defined(_MSC_FULL_VER)
#    define LZO_INFO_CCVER      LZO_PP_MACRO_EXPAND(_MSC_VER) "." LZO_PP_MACRO_EXPAND(_MSC_FULL_VER)
#  else
#    define LZO_INFO_CCVER      LZO_PP_MACRO_EXPAND(_MSC_VER)
#  endif
#elif defined(__MWERKS__)
#  define LZO_CC_MWERKS         1
#  define LZO_INFO_CC           "Metrowerks C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__MWERKS__)
#elif (defined(__NDPC__) || defined(__NDPX__)) && defined(__i386)
#  define LZO_CC_NDPC           1
#  define LZO_INFO_CC           "Microway NDP C"
#  define LZO_INFO_CCVER        "unknown"
#elif defined(__PACIFIC__)
#  define LZO_CC_PACIFICC       1
#  define LZO_INFO_CC           "Pacific C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__PACIFIC__)
#elif defined(__PGI) && (defined(__linux__) || defined(__WIN32__))
#  define LZO_CC_PGI            1
#  define LZO_INFO_CC           "Portland Group PGI C"
#  define LZO_INFO_CCVER        "unknown"
#elif defined(__PUREC__) && defined(__TOS__)
#  define LZO_CC_PUREC          1
#  define LZO_INFO_CC           "Pure C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__PUREC__)
#elif defined(__SC__) && defined(__ZTC__)
#  define LZO_CC_SYMANTECC      1
#  define LZO_INFO_CC           "Symantec C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__SC__)
#elif defined(__SUNPRO_C)
#  define LZO_INFO_CC           "SunPro C"
#  if ((__SUNPRO_C)+0 > 0)
#    define LZO_CC_SUNPROC      __SUNPRO_C
#    define LZO_INFO_CCVER      LZO_PP_MACRO_EXPAND(__SUNPRO_C)
#  else
#    define LZO_CC_SUNPROC      1
#    define LZO_INFO_CCVER      "unknown"
#  endif
#elif defined(__SUNPRO_CC)
#  define LZO_INFO_CC           "SunPro C"
#  if ((__SUNPRO_CC)+0 > 0)
#    define LZO_CC_SUNPROC      __SUNPRO_CC
#    define LZO_INFO_CCVER      LZO_PP_MACRO_EXPAND(__SUNPRO_CC)
#  else
#    define LZO_CC_SUNPROC      1
#    define LZO_INFO_CCVER      "unknown"
#  endif
#elif defined(__TINYC__)
#  define LZO_CC_TINYC          1
#  define LZO_INFO_CC           "Tiny C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__TINYC__)
#elif defined(__TSC__)
#  define LZO_CC_TOPSPEEDC      1
#  define LZO_INFO_CC           "TopSpeed C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__TSC__)
#elif defined(__WATCOMC__)
#  define LZO_CC_WATCOMC        1
#  define LZO_INFO_CC           "Watcom C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__WATCOMC__)
#elif defined(__TURBOC__)
#  define LZO_CC_TURBOC         1
#  define LZO_INFO_CC           "Turbo C"
#  define LZO_INFO_CCVER        LZO_PP_MACRO_EXPAND(__TURBOC__)
#elif defined(__ZTC__)
#  define LZO_CC_ZORTECHC       1
#  define LZO_INFO_CC           "Zortech C"
#  if (__ZTC__ == 0x310)
#    define LZO_INFO_CCVER      "0x310"
#  else
#    define LZO_INFO_CCVER      LZO_PP_MACRO_EXPAND(__ZTC__)
#  endif
#else
#  define LZO_CC_UNKNOWN        1
#  define LZO_INFO_CC           "unknown"
#  define LZO_INFO_CCVER        "unknown"
#endif
#if !defined(__LZO_ARCH_OVERRIDE) && !defined(LZO_ARCH_GENERIC) && defined(_CRAY)
#  if (UINT_MAX > LZO_0xffffffffL) && defined(_CRAY)
#    if defined(_CRAYMPP) || defined(_CRAYT3D) || defined(_CRAYT3E)
#      define LZO_ARCH_CRAY_MPP     1
#    endif
#  endif
#endif
#if !defined(__LZO_ARCH_OVERRIDE)
#if defined(LZO_ARCH_GENERIC)
#  define LZO_INFO_ARCH             "generic"
#elif defined(__alpha__) || defined(__alpha) || defined(_M_ALPHA)
#  define LZO_ARCH_ALPHA            1
#  define LZO_INFO_ARCH             "alpha"
#elif (LZO_ARCH_CRAY_MPP) && (defined(_CRAYT3D) || defined(_CRAYT3E))
#  define LZO_ARCH_ALPHA            1
#  define LZO_INFO_ARCH             "alpha"
#elif defined(__amd64__) || defined(__x86_64__) || defined(_M_AMD64)
#  define LZO_ARCH_AMD64            1
#  define LZO_INFO_ARCH             "amd64"
#elif defined(__thumb__) || (defined(_M_ARM) && defined(_M_THUMB))
#  define LZO_ARCH_ARM              1
#  define LZO_ARCH_ARM_THUMB        1
#  define LZO_INFO_ARCH             "arm_thumb"
#elif defined(__IAR_SYSTEMS_ICC__) && defined(__ICCARM__)
#  define LZO_ARCH_ARM              1
#  if defined(__CPU_MODE__) && ((__CPU_MODE__)+0 == 1)
#    define LZO_ARCH_ARM_THUMB      1
#    define LZO_INFO_ARCH           "arm_thumb"
#  elif defined(__CPU_MODE__) && ((__CPU_MODE__)+0 == 2)
#    define LZO_INFO_ARCH           "arm"
#  else
#    define LZO_INFO_ARCH           "arm"
#  endif
#elif defined(__arm__) || defined(_M_ARM)
#  define LZO_ARCH_ARM              1
#  define LZO_INFO_ARCH             "arm"
#elif defined(__bfin__)
#  define LZO_ARCH_BLACKFIN         1
#  define LZO_INFO_ARCH             "blackfin"
#elif defined(__cris__)
#  define LZO_ARCH_CRIS             1
#  define LZO_INFO_ARCH             "cris"
#elif defined(__IAR_SYSTEMS_ICC__) && defined(__ICCEZ80__)
#  define LZO_ARCH_EZ80             1
#  define LZO_INFO_ARCH             "ez80"
#elif defined(__H8300__) || defined(__H8300H__) || defined(__H8300S__) || defined(__H8300SX__)
#  define LZO_ARCH_H8300            1
#  define LZO_INFO_ARCH             "h8300"
#elif defined(__hppa__) || defined(__hppa)
#  define LZO_ARCH_HPPA             1
#  define LZO_INFO_ARCH             "hppa"
#elif defined(__386__) || defined(__i386__) || defined(__i386) || defined(_M_IX86) || defined(_M_I386)
#  define LZO_ARCH_I386             1
#  define LZO_ARCH_IA32             1
#  define LZO_INFO_ARCH             "i386"
#elif (LZO_CC_ZORTECHC && defined(__I86__))
#  define LZO_ARCH_I386             1
#  define LZO_ARCH_IA32             1
#  define LZO_INFO_ARCH             "i386"
#elif (LZO_OS_DOS32 && LZO_CC_HIGHC) && defined(_I386)
#  define LZO_ARCH_I386             1
#  define LZO_ARCH_IA32             1
#  define LZO_INFO_ARCH             "i386"
#elif defined(__ia64__) || defined(__ia64) || defined(_M_IA64)
#  define LZO_ARCH_IA64             1
#  define LZO_INFO_ARCH             "ia64"
#elif defined(__m32r__)
#  define LZO_ARCH_M32R             1
#  define LZO_INFO_ARCH             "m32r"
#elif (LZO_OS_TOS) || defined(__m68k__) || defined(__m68000__) || defined(__mc68000__) || defined(__mc68020__) || defined(_M_M68K)
#  define LZO_ARCH_M68K             1
#  define LZO_INFO_ARCH             "m68k"
#elif defined(__mips__) || defined(__mips) || defined(_MIPS_ARCH) || defined(_M_MRX000)
#  define LZO_ARCH_MIPS             1
#  define LZO_INFO_ARCH             "mips"
#elif defined(__powerpc__) || defined(__powerpc) || defined(__ppc__) || defined(__PPC__) || defined(_M_PPC) || defined(_ARCH_PPC) || defined(_ARCH_PWR)
#  define LZO_ARCH_POWERPC          1
#  define LZO_INFO_ARCH             "powerpc"
#elif defined(__s390__) || defined(__s390) || defined(__s390x__) || defined(__s390x)
#  define LZO_ARCH_S390             1
#  define LZO_INFO_ARCH             "s390"
#elif defined(__sh__) || defined(_M_SH)
#  define LZO_ARCH_SH               1
#  define LZO_INFO_ARCH             "sh"
#elif defined(__sparc__) || defined(__sparc) || defined(__sparcv8)
#  define LZO_ARCH_SPARC            1
#  define LZO_INFO_ARCH             "sparc"
#elif defined(__SPU__)
#  define LZO_ARCH_SPU              1
#  define LZO_INFO_ARCH             "spu"
#else
#  define LZO_ARCH_UNKNOWN          1
#  define LZO_INFO_ARCH             "unknown"
#endif
#endif
#if (LZO_ARCH_UNKNOWN) && (LZO_OS_DOS32 || LZO_OS_OS2)
#  error "FIXME - missing define for CPU architecture"
#endif
#if (LZO_ARCH_UNKNOWN) && (LZO_OS_WIN32)
#  error "FIXME - missing WIN32 define for CPU architecture"
#endif
#if (LZO_ARCH_UNKNOWN) && (LZO_OS_WIN64)
#  error "FIXME - missing WIN64 define for CPU architecture"
#endif
#if defined(LZO_ARCH_ARM_THUMB) && !defined(LZO_ARCH_ARM)
#  error "this should not happen"
#endif
#if (LZO_ARCH_I386)
#  if (UINT_MAX != LZO_0xffffffffL)
#    error "this should not happen"
#  endif
#  if (ULONG_MAX != LZO_0xffffffffL)
#    error "this should not happen"
#  endif
#endif
#if defined(SIZEOF_SHORT)
#  define LZO_SIZEOF_SHORT          (SIZEOF_SHORT)
#endif
#if defined(SIZEOF_INT)
#  define LZO_SIZEOF_INT            (SIZEOF_INT)
#endif
#if defined(SIZEOF_LONG)
#  define LZO_SIZEOF_LONG           (SIZEOF_LONG)
#endif
#if defined(SIZEOF_LONG_LONG)
#  define LZO_SIZEOF_LONG_LONG      (SIZEOF_LONG_LONG)
#endif
#if defined(SIZEOF___INT16)
#  define LZO_SIZEOF___INT16        (SIZEOF___INT16)
#endif
#if defined(SIZEOF___INT32)
#  define LZO_SIZEOF___INT32        (SIZEOF___INT32)
#endif
#if defined(SIZEOF___INT64)
#  define LZO_SIZEOF___INT64        (SIZEOF___INT64)
#endif
#if defined(SIZEOF_VOID_P)
#  define LZO_SIZEOF_VOID_P         (SIZEOF_VOID_P)
#endif
#if defined(SIZEOF_SIZE_T)
#  define LZO_SIZEOF_SIZE_T         (SIZEOF_SIZE_T)
#endif
#if defined(SIZEOF_PTRDIFF_T)
#  define LZO_SIZEOF_PTRDIFF_T      (SIZEOF_PTRDIFF_T)
#endif
#define __LZO_LSR(x,b)    (((x)+0ul) >> (b))
#if !defined(LZO_SIZEOF_SHORT)
#  if (USHRT_MAX == LZO_0xffffL)
#    define LZO_SIZEOF_SHORT        2
#  elif (__LZO_LSR(USHRT_MAX,7) == 1)
#    define LZO_SIZEOF_SHORT        1
#  elif (__LZO_LSR(USHRT_MAX,15) == 1)
#    define LZO_SIZEOF_SHORT        2
#  elif (__LZO_LSR(USHRT_MAX,31) == 1)
#    define LZO_SIZEOF_SHORT        4
#  elif (__LZO_LSR(USHRT_MAX,63) == 1)
#    define LZO_SIZEOF_SHORT        8
#  elif (__LZO_LSR(USHRT_MAX,127) == 1)
#    define LZO_SIZEOF_SHORT        16
#  else
#    error "LZO_SIZEOF_SHORT"
#  endif
#endif
#if !defined(LZO_SIZEOF_INT)
#  if (UINT_MAX == LZO_0xffffL)
#    define LZO_SIZEOF_INT          2
#  elif (UINT_MAX == LZO_0xffffffffL)
#    define LZO_SIZEOF_INT          4
#  elif (__LZO_LSR(UINT_MAX,7) == 1)
#    define LZO_SIZEOF_INT          1
#  elif (__LZO_LSR(UINT_MAX,15) == 1)
#    define LZO_SIZEOF_INT          2
#  elif (__LZO_LSR(UINT_MAX,31) == 1)
#    define LZO_SIZEOF_INT          4
#  elif (__LZO_LSR(UINT_MAX,63) == 1)
#    define LZO_SIZEOF_INT          8
#  elif (__LZO_LSR(UINT_MAX,127) == 1)
#    define LZO_SIZEOF_INT          16
#  else
#    error "LZO_SIZEOF_INT"
#  endif
#endif
#if !defined(LZO_SIZEOF_LONG)
#  if (ULONG_MAX == LZO_0xffffffffL)
#    define LZO_SIZEOF_LONG         4
#  elif (__LZO_LSR(ULONG_MAX,7) == 1)
#    define LZO_SIZEOF_LONG         1
#  elif (__LZO_LSR(ULONG_MAX,15) == 1)
#    define LZO_SIZEOF_LONG         2
#  elif (__LZO_LSR(ULONG_MAX,31) == 1)
#    define LZO_SIZEOF_LONG         4
#  elif (__LZO_LSR(ULONG_MAX,63) == 1)
#    define LZO_SIZEOF_LONG         8
#  elif (__LZO_LSR(ULONG_MAX,127) == 1)
#    define LZO_SIZEOF_LONG         16
#  else
#    error "LZO_SIZEOF_LONG"
#  endif
#endif
#if !defined(LZO_SIZEOF_LONG_LONG) && !defined(LZO_SIZEOF___INT64)
#if (LZO_SIZEOF_LONG > 0 && LZO_SIZEOF_LONG < 8)
#  if defined(__LONG_MAX__) && defined(__LONG_LONG_MAX__)
#    if (LZO_CC_GNUC >= 0x030300ul)
#      if ((__LONG_MAX__)+0 == (__LONG_LONG_MAX__)+0)
#        define LZO_SIZEOF_LONG_LONG      LZO_SIZEOF_LONG
#      elif (__LZO_LSR(__LONG_LONG_MAX__,30) == 1)
#        define LZO_SIZEOF_LONG_LONG      4
#      endif
#    endif
#  endif
#endif
#endif
#if !defined(LZO_SIZEOF_LONG_LONG) && !defined(LZO_SIZEOF___INT64)
#if (LZO_SIZEOF_LONG > 0 && LZO_SIZEOF_LONG < 8)
#if (LZO_CC_CILLY) && defined(__GNUC__)
#  define LZO_SIZEOF_LONG_LONG      8
#elif (LZO_CC_GNUC || LZO_CC_LLVM || LZO_CC_PATHSCALE)
#  define LZO_SIZEOF_LONG_LONG      8
#elif ((LZO_OS_WIN32 || LZO_OS_WIN64 || defined(_WIN32)) && LZO_CC_MSC && (_MSC_VER >= 1400))
#  define LZO_SIZEOF_LONG_LONG      8
#elif (LZO_OS_WIN64 || defined(_WIN64))
#  define LZO_SIZEOF___INT64        8
#elif (LZO_ARCH_I386 && (LZO_CC_DMC))
#  define LZO_SIZEOF_LONG_LONG      8
#elif (LZO_ARCH_I386 && (LZO_CC_SYMANTECC && (__SC__ >= 0x700)))
#  define LZO_SIZEOF_LONG_LONG      8
#elif (LZO_ARCH_I386 && (LZO_CC_INTELC && defined(__linux__)))
#  define LZO_SIZEOF_LONG_LONG      8
#elif (LZO_ARCH_I386 && (LZO_CC_MWERKS || LZO_CC_PELLESC || LZO_CC_PGI || LZO_CC_SUNPROC))
#  define LZO_SIZEOF_LONG_LONG      8
#elif (LZO_ARCH_I386 && (LZO_CC_INTELC || LZO_CC_MSC))
#  define LZO_SIZEOF___INT64        8
#elif ((LZO_OS_WIN32 || defined(_WIN32)) && (LZO_CC_MSC))
#  define LZO_SIZEOF___INT64        8
#elif (LZO_ARCH_I386 && (LZO_CC_BORLANDC && (__BORLANDC__ >= 0x0520)))
#  define LZO_SIZEOF___INT64        8
#elif (LZO_ARCH_I386 && (LZO_CC_WATCOMC && (__WATCOMC__ >= 1100)))
#  define LZO_SIZEOF___INT64        8
#elif (LZO_CC_WATCOMC && defined(_INTEGRAL_MAX_BITS) && (_INTEGRAL_MAX_BITS == 64))
#  define LZO_SIZEOF___INT64        8
#elif (LZO_OS_OS400 || defined(__OS400__)) && defined(__LLP64_IFC__)
#  define LZO_SIZEOF_LONG_LONG      8
#elif (defined(__vms) || defined(__VMS)) && (__INITIAL_POINTER_SIZE+0 == 64)
#  define LZO_SIZEOF_LONG_LONG      8
#elif (LZO_CC_SDCC) && (LZO_SIZEOF_INT == 2)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#  define LZO_SIZEOF_LONG_LONG      8
#endif
#endif
#endif
#if defined(__cplusplus) && defined(LZO_CC_GNUC)
#  if (LZO_CC_GNUC < 0x020800ul)
#    undef LZO_SIZEOF_LONG_LONG
#  endif
#endif
#if defined(LZO_CFG_NO_LONG_LONG) || defined(__NO_LONG_LONG)
#  undef LZO_SIZEOF_LONG_LONG
#endif
#if !defined(LZO_SIZEOF_VOID_P)
#if (LZO_ARCH_H8300)
#  if defined(__NORMAL_MODE__)
#    define __LZO_WORDSIZE          4
#    define LZO_SIZEOF_VOID_P       2
#  elif defined(__H8300H__) || defined(__H8300S__) || defined(__H8300SX__)
#    define __LZO_WORDSIZE          4
#    define LZO_SIZEOF_VOID_P       4
#  else
#    define __LZO_WORDSIZE          2
#    define LZO_SIZEOF_VOID_P       2
#  endif
#  if (LZO_CC_GNUC && (LZO_CC_GNUC < 0x040000ul)) && (LZO_SIZEOF_INT == 4)
#    define LZO_SIZEOF_SIZE_T       LZO_SIZEOF_INT
#    define LZO_SIZEOF_PTRDIFF_T    LZO_SIZEOF_INT
#  endif
#elif (LZO_SIZEOF_LONG == 8) && ((defined(__mips__) && defined(__R5900__)) || defined(__MIPS_PSX2__))
#  define __LZO_WORDSIZE            8
#  define LZO_SIZEOF_VOID_P         4
#elif defined(__LLP64__) || defined(__LLP64) || defined(_LLP64) || defined(_WIN64)
#  define __LZO_WORDSIZE            8
#  define LZO_SIZEOF_VOID_P         8
#elif (LZO_OS_OS400 || defined(__OS400__)) && defined(__LLP64_IFC__)
#  define LZO_SIZEOF_VOID_P         LZO_SIZEOF_LONG
#  define LZO_SIZEOF_SIZE_T         LZO_SIZEOF_LONG
#  define LZO_SIZEOF_PTRDIFF_T      LZO_SIZEOF_LONG
#elif (LZO_OS_OS400 || defined(__OS400__))
#  define __LZO_WORDSIZE            LZO_SIZEOF_LONG
#  define LZO_SIZEOF_VOID_P         16
#  define LZO_SIZEOF_SIZE_T         LZO_SIZEOF_LONG
#  define LZO_SIZEOF_PTRDIFF_T      LZO_SIZEOF_LONG
#elif (defined(__vms) || defined(__VMS)) && (__INITIAL_POINTER_SIZE+0 == 64)
#  define LZO_SIZEOF_VOID_P         8
#  define LZO_SIZEOF_SIZE_T         LZO_SIZEOF_LONG
#  define LZO_SIZEOF_PTRDIFF_T      LZO_SIZEOF_LONG
#elif (LZO_ARCH_SPU)
#  define LZO_SIZEOF_VOID_P         4
#else
#  define LZO_SIZEOF_VOID_P         LZO_SIZEOF_LONG
#endif
#endif
#if !defined(LZO_WORDSIZE)
#  if defined(__LZO_WORDSIZE)
#    define LZO_WORDSIZE            __LZO_WORDSIZE
#  else
#    define LZO_WORDSIZE            LZO_SIZEOF_VOID_P
#  endif
#endif
#if !defined(LZO_SIZEOF_SIZE_T)
#  define LZO_SIZEOF_SIZE_T         LZO_SIZEOF_VOID_P
#endif
#if !defined(LZO_SIZEOF_PTRDIFF_T)
#  define LZO_SIZEOF_PTRDIFF_T      LZO_SIZEOF_SIZE_T
#endif
#if defined(LZO_ABI_NEUTRAL_ENDIAN)
#  undef LZO_ABI_BIG_ENDIAN
#  undef LZO_ABI_LITTLE_ENDIAN
#elif !defined(LZO_ABI_BIG_ENDIAN) && !defined(LZO_ABI_LITTLE_ENDIAN)
#if (LZO_ARCH_ALPHA) && (LZO_ARCH_CRAY_MPP)
#  define LZO_ABI_BIG_ENDIAN        1
#elif (LZO_ARCH_ALPHA || LZO_ARCH_AMD64 || LZO_ARCH_BLACKFIN || LZO_ARCH_CRIS || LZO_ARCH_I386)
#  define LZO_ABI_LITTLE_ENDIAN     1
#elif (LZO_ARCH_M68K || LZO_ARCH_S390)
#  define LZO_ABI_BIG_ENDIAN        1
#elif defined(__IAR_SYSTEMS_ICC__) && defined(__LITTLE_ENDIAN__)
#  if (__LITTLE_ENDIAN__ == 1)
#    define LZO_ABI_LITTLE_ENDIAN   1
#  else
#    define LZO_ABI_BIG_ENDIAN      1
#  endif
#elif defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#  define LZO_ABI_BIG_ENDIAN        1
#elif defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#  define LZO_ABI_LITTLE_ENDIAN     1
#elif (LZO_ARCH_ARM) && defined(__ARMEB__) && !defined(__ARMEL__)
#  define LZO_ABI_BIG_ENDIAN        1
#elif (LZO_ARCH_ARM) && defined(__ARMEL__) && !defined(__ARMEB__)
#  define LZO_ABI_LITTLE_ENDIAN     1
#elif (LZO_ARCH_MIPS) && defined(__MIPSEB__) && !defined(__MIPSEL__)
#  define LZO_ABI_BIG_ENDIAN        1
#elif (LZO_ARCH_MIPS) && defined(__MIPSEL__) && !defined(__MIPSEB__)
#  define LZO_ABI_LITTLE_ENDIAN     1
#endif
#endif
#if defined(LZO_ABI_BIG_ENDIAN) && defined(LZO_ABI_LITTLE_ENDIAN)
#  error "this should not happen"
#endif
#if defined(LZO_ABI_BIG_ENDIAN)
#  define LZO_INFO_ABI_ENDIAN       "be"
#elif defined(LZO_ABI_LITTLE_ENDIAN)
#  define LZO_INFO_ABI_ENDIAN       "le"
#elif defined(LZO_ABI_NEUTRAL_ENDIAN)
#  define LZO_INFO_ABI_ENDIAN       "neutral"
#endif
#if (LZO_SIZEOF_INT == 1 && LZO_SIZEOF_LONG == 2 && LZO_SIZEOF_VOID_P == 2)
#  define LZO_ABI_I8LP16         1
#  define LZO_INFO_ABI_PM       "i8lp16"
#elif (LZO_SIZEOF_INT == 2 && LZO_SIZEOF_LONG == 2 && LZO_SIZEOF_VOID_P == 2)
#  define LZO_ABI_ILP16         1
#  define LZO_INFO_ABI_PM       "ilp16"
#elif (LZO_SIZEOF_INT == 4 && LZO_SIZEOF_LONG == 4 && LZO_SIZEOF_VOID_P == 4)
#  define LZO_ABI_ILP32         1
#  define LZO_INFO_ABI_PM       "ilp32"
#elif (LZO_SIZEOF_INT == 4 && LZO_SIZEOF_LONG == 4 && LZO_SIZEOF_VOID_P == 8 && LZO_SIZEOF_SIZE_T == 8)
#  define LZO_ABI_LLP64         1
#  define LZO_INFO_ABI_PM       "llp64"
#elif (LZO_SIZEOF_INT == 4 && LZO_SIZEOF_LONG == 8 && LZO_SIZEOF_VOID_P == 8)
#  define LZO_ABI_LP64          1
#  define LZO_INFO_ABI_PM       "lp64"
#elif (LZO_SIZEOF_INT == 8 && LZO_SIZEOF_LONG == 8 && LZO_SIZEOF_VOID_P == 8)
#  define LZO_ABI_ILP64         1
#  define LZO_INFO_ABI_PM       "ilp64"
#elif (LZO_SIZEOF_INT == 4 && LZO_SIZEOF_LONG == 8 && LZO_SIZEOF_VOID_P == 4)
#  define LZO_ABI_IP32L64       1
#  define LZO_INFO_ABI_PM       "ip32l64"
#endif
#if !defined(__LZO_LIBC_OVERRIDE)
#if defined(LZO_LIBC_NAKED)
#  define LZO_INFO_LIBC         "naked"
#elif defined(LZO_LIBC_FREESTANDING)
#  define LZO_INFO_LIBC         "freestanding"
#elif defined(LZO_LIBC_MOSTLY_FREESTANDING)
#  define LZO_INFO_LIBC         "mfreestanding"
#elif defined(LZO_LIBC_ISOC90)
#  define LZO_INFO_LIBC         "isoc90"
#elif defined(LZO_LIBC_ISOC99)
#  define LZO_INFO_LIBC         "isoc99"
#elif defined(__dietlibc__)
#  define LZO_LIBC_DIETLIBC     1
#  define LZO_INFO_LIBC         "dietlibc"
#elif defined(_NEWLIB_VERSION)
#  define LZO_LIBC_NEWLIB       1
#  define LZO_INFO_LIBC         "newlib"
#elif defined(__UCLIBC__) && defined(__UCLIBC_MAJOR__) && defined(__UCLIBC_MINOR__)
#  if defined(__UCLIBC_SUBLEVEL__)
#    define LZO_LIBC_UCLIBC     (__UCLIBC_MAJOR__ * 0x10000L + __UCLIBC_MINOR__ * 0x100 + __UCLIBC_SUBLEVEL__)
#  else
#    define LZO_LIBC_UCLIBC     0x00090bL
#  endif
#  define LZO_INFO_LIBC         "uclibc"
#elif defined(__GLIBC__) && defined(__GLIBC_MINOR__)
#  define LZO_LIBC_GLIBC        (__GLIBC__ * 0x10000L + __GLIBC_MINOR__ * 0x100)
#  define LZO_INFO_LIBC         "glibc"
#elif (LZO_CC_MWERKS) && defined(__MSL__)
#  define LZO_LIBC_MSL          __MSL__
#  define LZO_INFO_LIBC         "msl"
#elif defined(__IAR_SYSTEMS_ICC__)
#  define LZO_LIBC_ISOC90       1
#  define LZO_INFO_LIBC         "isoc90"
#else
#  define LZO_LIBC_DEFAULT      1
#  define LZO_INFO_LIBC         "default"
#endif
#endif
#if !defined(__lzo_likely) && !defined(__lzo_unlikely)
#if (LZO_CC_GNUC >= 0x030200ul)
#  define __lzo_likely(e)       (__builtin_expect(!!(e),1))
#  define __lzo_unlikely(e)     (__builtin_expect(!!(e),0))
#elif (LZO_CC_INTELC && (__INTEL_COMPILER >= 800))
#  define __lzo_likely(e)       (__builtin_expect(!!(e),1))
#  define __lzo_unlikely(e)     (__builtin_expect(!!(e),0))
#elif (LZO_CC_LLVM || LZO_CC_PATHSCALE)
#  define __lzo_likely(e)       (__builtin_expect(!!(e),1))
#  define __lzo_unlikely(e)     (__builtin_expect(!!(e),0))
#endif
#endif
#if defined(__lzo_likely)
#  define __lzo_HAVE_likely 1
#else
#  define __lzo_likely(e)       (e)
#endif
#if defined(__lzo_unlikely)
#  define __lzo_HAVE_unlikely 1
#else
#  define __lzo_unlikely(e)     (e)
#endif
#if !defined(LZO_UNUSED)
#  if (LZO_CC_BORLANDC && (__BORLANDC__ >= 0x0600))
#    define LZO_UNUSED(var)         ((void) &var)
#  elif (LZO_CC_BORLANDC || LZO_CC_HIGHC || LZO_CC_NDPC || LZO_CC_PELLESC || LZO_CC_TURBOC)
#    define LZO_UNUSED(var)         if (&var) ; else
#  elif (LZO_CC_GNUC || LZO_CC_LLVM || LZO_CC_PATHSCALE)
#    define LZO_UNUSED(var)         ((void) var)
#  elif (LZO_CC_MSC && (_MSC_VER < 900))
#    define LZO_UNUSED(var)         if (&var) ; else
#  elif (LZO_CC_KEILC)
#    define LZO_UNUSED(var)         {extern int __lzo_unused[1-2*!(sizeof(var)>0)];}
#  elif (LZO_CC_PACIFICC)
#    define LZO_UNUSED(var)         ((void) sizeof(var))
#  elif (LZO_CC_WATCOMC) && defined(__cplusplus)
#    define LZO_UNUSED(var)         ((void) var)
#  else
#    define LZO_UNUSED(var)         ((void) &var)
#  endif
#endif
#if !defined(LZO_COMPILE_TIME_ASSERT_HEADER)
#  if (LZO_CC_AZTECC || LZO_CC_ZORTECHC)
#    define LZO_COMPILE_TIME_ASSERT_HEADER(e)  extern int __lzo_cta[1-!(e)];
#  elif (LZO_CC_DMC || LZO_CC_SYMANTECC)
#    define LZO_COMPILE_TIME_ASSERT_HEADER(e)  extern int __lzo_cta[1u-2*!(e)];
#  elif (LZO_CC_TURBOC && (__TURBOC__ == 0x0295))
#    define LZO_COMPILE_TIME_ASSERT_HEADER(e)  extern int __lzo_cta[1-!(e)];
#  else
#    define LZO_COMPILE_TIME_ASSERT_HEADER(e)  extern int __lzo_cta[1-2*!(e)];
#  endif
#endif
#if !defined(LZO_CFG_NO_UNALIGNED)
#if defined(LZO_ABI_NEUTRAL_ENDIAN) || defined(LZO_ARCH_GENERIC)
#  define LZO_CFG_NO_UNALIGNED 1
#endif
#endif
#if defined(__LZO_INFOSTR_PM)
#elif defined(LZO_INFO_ABI_PM)
#  define __LZO_INFOSTR_PM          "." LZO_INFO_ABI_PM
#else
#  define __LZO_INFOSTR_PM          ""
#endif
#if defined(__LZO_INFOSTR_ENDIAN)
#elif defined(LZO_INFO_ABI_ENDIAN)
#  define __LZO_INFOSTR_ENDIAN      "." LZO_INFO_ABI_ENDIAN
#else
#  define __LZO_INFOSTR_ENDIAN      ""
#endif
#if defined(__LZO_INFOSTR_OSNAME)
#elif defined(LZO_INFO_OS_CONSOLE)
#  define __LZO_INFOSTR_OSNAME      LZO_INFO_OS "." LZO_INFO_OS_CONSOLE
#elif defined(LZO_INFO_OS_POSIX)
#  define __LZO_INFOSTR_OSNAME      LZO_INFO_OS "." LZO_INFO_OS_POSIX
#else
#  define __LZO_INFOSTR_OSNAME      LZO_INFO_OS
#endif
#if defined(__LZO_INFOSTR_LIBC)
#elif defined(LZO_INFO_LIBC)
#  define __LZO_INFOSTR_LIBC        "." LZO_INFO_LIBC
#else
#  define __LZO_INFOSTR_LIBC        ""
#endif
#if defined(__LZO_INFOSTR_CCVER)
#elif defined(LZO_INFO_CCVER)
#  define __LZO_INFOSTR_CCVER       " " LZO_INFO_CCVER
#else
#  define __LZO_INFOSTR_CCVER       ""
#endif
#define LZO_INFO_STRING \
    LZO_INFO_ARCH __LZO_INFOSTR_PM __LZO_INFOSTR_ENDIAN \
    " " __LZO_INFOSTR_OSNAME __LZO_INFOSTR_LIBC " " LZO_INFO_CC __LZO_INFOSTR_CCVER

#endif // LZODEFS_HH
