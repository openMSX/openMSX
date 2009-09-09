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
#define __LZO_MASK_GEN(o,b)     (((((o) << ((b)-1)) - (o)) << 1) + (o))
#if defined(__cplusplus)
#  if !defined(__STDC_CONSTANT_MACROS)
#    define __STDC_CONSTANT_MACROS 1
#  endif
#  if !defined(__STDC_LIMIT_MACROS)
#    define __STDC_LIMIT_MACROS 1
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
#define __LZO_LSR(x,b)    (((x)+0ul) >> (b))

#endif // LZODEFS_HH
