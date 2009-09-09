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
#elif defined(LZO_OS_EMBEDDED)
#elif defined(__IAR_SYSTEMS_ICC__)
#  define LZO_OS_EMBEDDED       1
#elif defined(__CYGWIN__) && defined(__GNUC__)
#  define LZO_OS_CYGWIN         1
#elif defined(__EMX__) && defined(__GNUC__)
#  define LZO_OS_EMX            1
#elif defined(__BEOS__)
#  define LZO_OS_BEOS           1
#elif defined(__Lynx__)
#  define LZO_OS_LYNXOS         1
#elif defined(__OS400__)
#  define LZO_OS_OS400          1
#elif defined(__QNX__)
#  define LZO_OS_QNX            1
#elif defined(__BORLANDC__) && defined(__DPMI32__) && (__BORLANDC__ >= 0x0460)
#  define LZO_OS_DOS32          1
#elif defined(__ZTC__) && defined(DOS386)
#  define LZO_OS_DOS32          1
#elif defined(__OS2__) || defined(__OS2V2__)
#    define LZO_OS_OS2          1
#elif defined(__WIN64__) || defined(_WIN64) || defined(WIN64)
#  define LZO_OS_WIN64          1
#elif defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WINDOWS_386__)
#  define LZO_OS_WIN32          1
#elif defined(__MWERKS__) && defined(__INTEL__)
#  define LZO_OS_WIN32          1
#elif defined(__WINDOWS__) || defined(_WINDOWS) || defined(_Windows)
#    define LZO_OS_WIN32        1
#elif defined(__DOS__) || defined(__MSDOS__) || defined(_MSDOS) || defined(MSDOS) || (defined(__PACIFIC__) && defined(DOS))
#    define LZO_OS_DOS32        1
#elif defined(__WATCOMC__)
#  if defined(__NT__) && (__WATCOMC__ < 1100)
#    define LZO_OS_WIN32        1
#  elif defined(__linux__) || defined(__LINUX__)
#    define LZO_OS_POSIX        1
#  else
#    error "please specify a target using the -bt compiler option"
#  endif
#elif defined(__palmos__)
#  define LZO_OS_PALMOS         1
#elif defined(__TOS__) || defined(__atarist__)
#  define LZO_OS_TOS            1
#elif defined(macintosh) && !defined(__ppc__)
#  define LZO_OS_MACCLASSIC     1
#elif defined(__VMS)
#  define LZO_OS_VMS            1
#elif ((defined(__mips__) && defined(__R5900__)) || defined(__MIPS_PSX2__))
#  define LZO_OS_CONSOLE        1
#  define LZO_OS_CONSOLE_PS2    1
#elif (defined(__mips__) && defined(__psp__))
#  define LZO_OS_CONSOLE        1
#  define LZO_OS_CONSOLE_PSP    1
#else
#  define LZO_OS_POSIX          1
#endif
#if (LZO_OS_POSIX)
#  if defined(_AIX) || defined(__AIX__) || defined(__aix__)
#    define LZO_OS_POSIX_AIX        1
#  elif defined(__FreeBSD__)
#    define LZO_OS_POSIX_FREEBSD    1
#  elif defined(__hpux__) || defined(__hpux)
#    define LZO_OS_POSIX_HPUX       1
#  elif defined(__INTERIX)
#    define LZO_OS_POSIX_INTERIX    1
#  elif defined(__IRIX__) || defined(__irix__)
#    define LZO_OS_POSIX_IRIX       1
#  elif defined(__linux__) || defined(__linux) || defined(__LINUX__)
#    define LZO_OS_POSIX_LINUX      1
#  elif defined(__APPLE__) || defined(__MACOS__)
#    define LZO_OS_POSIX_MACOSX     1
#  elif defined(__minix__) || defined(__minix)
#    define LZO_OS_POSIX_MINIX      1
#  elif defined(__NetBSD__)
#    define LZO_OS_POSIX_NETBSD     1
#  elif defined(__OpenBSD__)
#    define LZO_OS_POSIX_OPENBSD    1
#  elif defined(__osf__)
#    define LZO_OS_POSIX_OSF        1
#  elif defined(__solaris__) || defined(__sun)
#    if defined(__SVR4) || defined(__svr4__)
#      define LZO_OS_POSIX_SOLARIS  1
#    else
#      define LZO_OS_POSIX_SUNOS    1
#    endif
#  elif defined(__ultrix__) || defined(__ultrix)
#    define LZO_OS_POSIX_ULTRIX     1
#  elif defined(_UNICOS)
#    define LZO_OS_POSIX_UNICOS     1
#  else
#    define LZO_OS_POSIX_UNKNOWN    1
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
#  if defined(__CILLY__)
#  else
#  endif
#elif defined(__PATHSCALE__) && defined(__PATHCC_PATCHLEVEL__)
#  define LZO_CC_PATHSCALE      (__PATHCC__ * 0x10000L + __PATHCC_MINOR__ * 0x100 + __PATHCC_PATCHLEVEL__)
#elif defined(__INTEL_COMPILER)
#  define LZO_CC_INTELC         1
#  if defined(_WIN32) || defined(_WIN64)
#    define LZO_CC_SYNTAX_MSC 1
#  else
#    define LZO_CC_SYNTAX_GNUC 1
#  endif
#elif defined(__POCC__) && defined(_WIN32)
#  define LZO_CC_PELLESC        1
#elif defined(__llvm__) && defined(__GNUC__) && defined(__VERSION__)
#  if defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#    define LZO_CC_LLVM         (__GNUC__ * 0x10000L + __GNUC_MINOR__ * 0x100 + __GNUC_PATCHLEVEL__)
#  else
#    define LZO_CC_LLVM         (__GNUC__ * 0x10000L + __GNUC_MINOR__ * 0x100)
#  endif
#elif defined(__GNUC__) && defined(__VERSION__)
#  if defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#    define LZO_CC_GNUC         (__GNUC__ * 0x10000L + __GNUC_MINOR__ * 0x100 + __GNUC_PATCHLEVEL__)
#  elif defined(__GNUC_MINOR__)
#    define LZO_CC_GNUC         (__GNUC__ * 0x10000L + __GNUC_MINOR__ * 0x100)
#  else
#    define LZO_CC_GNUC         (__GNUC__ * 0x10000L)
#  endif
#elif defined(__ACK__) && defined(_ACK)
#  define LZO_CC_ACK            1
#elif defined(__AZTEC_C__)
#  define LZO_CC_AZTECC         1
#elif defined(__BORLANDC__)
#  define LZO_CC_BORLANDC       1
#elif defined(_CRAYC) && defined(_RELEASE)
#  define LZO_CC_CRAYC          1
#elif defined(__DMC__) && defined(__SC__)
#  define LZO_CC_DMC            1
#elif defined(__DECC)
#  define LZO_CC_DECC           1
#elif defined(__HIGHC__)
#  define LZO_CC_HIGHC          1
#elif defined(__IAR_SYSTEMS_ICC__)
#  define LZO_CC_IARC           1
#  if defined(__VER__)
#  else
#  endif
#elif defined(__IBMC__)
#  define LZO_CC_IBMC           1
#elif defined(__KEIL__) && defined(__C166__)
#  define LZO_CC_KEILC          1
#elif defined(__LCC__) && defined(_WIN32) && defined(__LCCOPTIMLEVEL)
#  define LZO_CC_LCCWIN32       1
#elif defined(__LCC__)
#  define LZO_CC_LCC            1
#  if defined(__LCC_VERSION__)
#  else
#  endif
#elif defined(_MSC_VER)
#  define LZO_CC_MSC            1
#  if defined(_MSC_FULL_VER)
#  else
#  endif
#elif defined(__MWERKS__)
#  define LZO_CC_MWERKS         1
#elif (defined(__NDPC__) || defined(__NDPX__)) && defined(__i386)
#  define LZO_CC_NDPC           1
#elif defined(__PACIFIC__)
#  define LZO_CC_PACIFICC       1
#elif defined(__PGI) && (defined(__linux__) || defined(__WIN32__))
#  define LZO_CC_PGI            1
#elif defined(__PUREC__) && defined(__TOS__)
#  define LZO_CC_PUREC          1
#elif defined(__SC__) && defined(__ZTC__)
#  define LZO_CC_SYMANTECC      1
#elif defined(__SUNPRO_C)
#  if ((__SUNPRO_C)+0 > 0)
#    define LZO_CC_SUNPROC      __SUNPRO_C
#  else
#    define LZO_CC_SUNPROC      1
#  endif
#elif defined(__SUNPRO_CC)
#  if ((__SUNPRO_CC)+0 > 0)
#    define LZO_CC_SUNPROC      __SUNPRO_CC
#  else
#    define LZO_CC_SUNPROC      1
#  endif
#elif defined(__TINYC__)
#  define LZO_CC_TINYC          1
#elif defined(__TSC__)
#  define LZO_CC_TOPSPEEDC      1
#elif defined(__WATCOMC__)
#  define LZO_CC_WATCOMC        1
#elif defined(__TURBOC__)
#  define LZO_CC_TURBOC         1
#elif defined(__ZTC__)
#  define LZO_CC_ZORTECHC       1
#  if (__ZTC__ == 0x310)
#  else
#  endif
#else
#  define LZO_CC_UNKNOWN        1
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
#elif defined(__alpha__) || defined(__alpha) || defined(_M_ALPHA)
#  define LZO_ARCH_ALPHA            1
#elif (LZO_ARCH_CRAY_MPP) && (defined(_CRAYT3D) || defined(_CRAYT3E))
#  define LZO_ARCH_ALPHA            1
#elif defined(__amd64__) || defined(__x86_64__) || defined(_M_AMD64)
#  define LZO_ARCH_AMD64            1
#elif defined(__thumb__) || (defined(_M_ARM) && defined(_M_THUMB))
#  define LZO_ARCH_ARM              1
#  define LZO_ARCH_ARM_THUMB        1
#elif defined(__IAR_SYSTEMS_ICC__) && defined(__ICCARM__)
#  define LZO_ARCH_ARM              1
#  if defined(__CPU_MODE__) && ((__CPU_MODE__)+0 == 1)
#    define LZO_ARCH_ARM_THUMB      1
#  elif defined(__CPU_MODE__) && ((__CPU_MODE__)+0 == 2)
#  else
#  endif
#elif defined(__arm__) || defined(_M_ARM)
#  define LZO_ARCH_ARM              1
#elif defined(__bfin__)
#  define LZO_ARCH_BLACKFIN         1
#elif defined(__cris__)
#  define LZO_ARCH_CRIS             1
#elif defined(__IAR_SYSTEMS_ICC__) && defined(__ICCEZ80__)
#  define LZO_ARCH_EZ80             1
#elif defined(__H8300__) || defined(__H8300H__) || defined(__H8300S__) || defined(__H8300SX__)
#  define LZO_ARCH_H8300            1
#elif defined(__hppa__) || defined(__hppa)
#  define LZO_ARCH_HPPA             1
#elif defined(__386__) || defined(__i386__) || defined(__i386) || defined(_M_IX86) || defined(_M_I386)
#  define LZO_ARCH_I386             1
#  define LZO_ARCH_IA32             1
#elif (LZO_CC_ZORTECHC && defined(__I86__))
#  define LZO_ARCH_I386             1
#  define LZO_ARCH_IA32             1
#elif (LZO_OS_DOS32 && LZO_CC_HIGHC) && defined(_I386)
#  define LZO_ARCH_I386             1
#  define LZO_ARCH_IA32             1
#elif defined(__ia64__) || defined(__ia64) || defined(_M_IA64)
#  define LZO_ARCH_IA64             1
#elif defined(__m32r__)
#  define LZO_ARCH_M32R             1
#elif (LZO_OS_TOS) || defined(__m68k__) || defined(__m68000__) || defined(__mc68000__) || defined(__mc68020__) || defined(_M_M68K)
#  define LZO_ARCH_M68K             1
#elif defined(__mips__) || defined(__mips) || defined(_MIPS_ARCH) || defined(_M_MRX000)
#  define LZO_ARCH_MIPS             1
#elif defined(__powerpc__) || defined(__powerpc) || defined(__ppc__) || defined(__PPC__) || defined(_M_PPC) || defined(_ARCH_PPC) || defined(_ARCH_PWR)
#  define LZO_ARCH_POWERPC          1
#elif defined(__s390__) || defined(__s390) || defined(__s390x__) || defined(__s390x)
#  define LZO_ARCH_S390             1
#elif defined(__sh__) || defined(_M_SH)
#  define LZO_ARCH_SH               1
#elif defined(__sparc__) || defined(__sparc) || defined(__sparcv8)
#  define LZO_ARCH_SPARC            1
#elif defined(__SPU__)
#  define LZO_ARCH_SPU              1
#else
#  define LZO_ARCH_UNKNOWN          1
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

#endif // LZODEFS_HH
