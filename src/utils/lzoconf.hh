// $Id$

/* lzoconf.hh -- configuration for the LZO real-time data compression library

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


#ifndef LZOCONF_HH
#define LZOCONF_HH

#define LZO_VERSION             0x2030
#define LZO_VERSION_STRING      "2.03"
#define LZO_VERSION_DATE        "Apr 30 2008"

#include <limits.h>
#include <stddef.h>


/***********************************************************************
// LZO requires a conforming <limits.h>
************************************************************************/

#if !defined(CHAR_BIT) || (CHAR_BIT != 8)
#  error "invalid CHAR_BIT"
#endif
#if !defined(UCHAR_MAX) || !defined(UINT_MAX) || !defined(ULONG_MAX)
#  error "check your compiler installation"
#endif
#if (USHRT_MAX < 1) || (UINT_MAX < 1) || (ULONG_MAX < 1)
#  error "your limits.h macros are broken"
#endif

/* get OS and architecture defines */
#include "lzodefs.hh"


/***********************************************************************
// integral and pointer types
************************************************************************/

/* lzo_uint should match size_t */
#if !defined(LZO_UINT_MAX)
#  if defined(LZO_ABI_LLP64) /* WIN64 */
#    if defined(LZO_OS_WIN64)
     typedef unsigned __int64   lzo_uint;
     typedef __int64            lzo_int;
#    else
     typedef unsigned long long lzo_uint;
     typedef long long          lzo_int;
#    endif
#    define LZO_UINT_MAX        0xffffffffffffffffull
#    define LZO_INT_MAX         9223372036854775807LL
#    define LZO_INT_MIN         (-1LL - LZO_INT_MAX)
#  elif defined(LZO_ABI_IP32L64) /* MIPS R5900 */
     typedef unsigned int       lzo_uint;
     typedef int                lzo_int;
#    define LZO_UINT_MAX        UINT_MAX
#    define LZO_INT_MAX         INT_MAX
#    define LZO_INT_MIN         INT_MIN
#  elif (ULONG_MAX >= LZO_0xffffffffL)
     typedef unsigned long      lzo_uint;
     typedef long               lzo_int;
#    define LZO_UINT_MAX        ULONG_MAX
#    define LZO_INT_MAX         LONG_MAX
#    define LZO_INT_MIN         LONG_MIN
#  else
#    error "lzo_uint"
#  endif
#endif

/* Integral types with 32 bits or more. */
#if !defined(LZO_UINT32_MAX)
#  if (UINT_MAX >= LZO_0xffffffffL)
     typedef unsigned int       lzo_uint32;
     typedef int                lzo_int32;
#    define LZO_UINT32_MAX      UINT_MAX
#    define LZO_INT32_MAX       INT_MAX
#    define LZO_INT32_MIN       INT_MIN
#  elif (ULONG_MAX >= LZO_0xffffffffL)
     typedef unsigned long      lzo_uint32;
     typedef long               lzo_int32;
#    define LZO_UINT32_MAX      ULONG_MAX
#    define LZO_INT32_MAX       LONG_MAX
#    define LZO_INT32_MIN       LONG_MIN
#  else
#    error "lzo_uint32"
#  endif
#endif

/* The larger type of lzo_uint and lzo_uint32. */
#if (LZO_UINT_MAX >= LZO_UINT32_MAX)
#  define lzo_xint              lzo_uint
#else
#  define lzo_xint              lzo_uint32
#endif

/* no typedef here because of const-pointer issues */
#define lzo_bytep               unsigned char *
#define lzo_charp               char *
#define lzo_voidp               void *
#define lzo_shortp              short *
#define lzo_ushortp             unsigned short *
#define lzo_uint32p             lzo_uint32 *
#define lzo_int32p              lzo_int32 *
#define lzo_uintp               lzo_uint *
#define lzo_intp                lzo_int *
#define lzo_xintp               lzo_xint *
#define lzo_voidpp              lzo_voidp *
#define lzo_bytepp              lzo_bytep *
/* deprecated - use `lzo_bytep' instead of `lzo_byte *' */
#define lzo_byte                unsigned char

typedef int lzo_bool;


/***********************************************************************
// error codes and prototypes
************************************************************************/

/* Error codes for the compression/decompression functions. Negative
 * values are errors, positive values will be used for special but
 * normal events.
 */
#define LZO_E_OK                    0
#define LZO_E_ERROR                 (-1)
#define LZO_E_OUT_OF_MEMORY         (-2)    /* [not used right now] */
#define LZO_E_NOT_COMPRESSIBLE      (-3)    /* [not used right now] */
#define LZO_E_INPUT_OVERRUN         (-4)
#define LZO_E_OUTPUT_OVERRUN        (-5)
#define LZO_E_LOOKBEHIND_OVERRUN    (-6)
#define LZO_E_EOF_NOT_FOUND         (-7)
#define LZO_E_INPUT_NOT_CONSUMED    (-8)
#define LZO_E_NOT_YET_IMPLEMENTED   (-9)    /* [not used right now] */


#ifndef lzo_sizeof_dict_t
#  define lzo_sizeof_dict_t     ((unsigned)sizeof(lzo_bytep))
#endif

/* lzo_init() should be the first function you call.
 * Check the return code !
 *
 * lzo_init() is a macro to allow checking that the library and the
 * compiler's view of various types are consistent.
 */
#define lzo_init() __lzo_init_v2(LZO_VERSION,(int)sizeof(short),(int)sizeof(int),\
    (int)sizeof(long),(int)sizeof(lzo_uint32),(int)sizeof(lzo_uint),\
    (int)lzo_sizeof_dict_t,(int)sizeof(char *),(int)sizeof(lzo_voidp))
int __lzo_init_v2(unsigned,int,int,int,int,int,int,int,int);

/* misc. */
int _lzo_config_check(void);

#endif // LZOCONF_HH
