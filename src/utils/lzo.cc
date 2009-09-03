// $Id$

/* minilzo.cc -- mini subset of the LZO real-time data compression library

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

/*
 * NOTE:
 *   the full LZO package can be found at
 *   http://www.oberhumer.com/opensource/lzo/
 */

#include <limits.h>
#include <stddef.h>

#include "lzo.hh"

// Start of configuration.

#if !defined(lzo_uintptr_t)
#  if defined(LZO_OS_OS400) && (LZO_SIZEOF_VOID_P == 16)
#    define __LZO_UINTPTR_T_IS_POINTER 1
     typedef char*              lzo_uintptr_t;
#    define lzo_uintptr_t       lzo_uintptr_t
#  elif (LZO_SIZEOF_SIZE_T == LZO_SIZEOF_VOID_P)
#    define lzo_uintptr_t       size_t
#  elif (LZO_SIZEOF_LONG == LZO_SIZEOF_VOID_P)
#    define lzo_uintptr_t       unsigned long
#  elif (LZO_SIZEOF_INT == LZO_SIZEOF_VOID_P)
#    define lzo_uintptr_t       unsigned int
#  elif (LZO_SIZEOF_LONG_LONG == LZO_SIZEOF_VOID_P)
#    define lzo_uintptr_t       unsigned long long
#  else
#    define lzo_uintptr_t       size_t
#  endif
#endif
LZO_COMPILE_TIME_ASSERT_HEADER(sizeof(lzo_uintptr_t) >= sizeof(lzo_voidp))

#include <string.h>

#undef NDEBUG
#  if !defined(LZO_DEBUG)
#    define NDEBUG 1
#  endif
#  include <assert.h>

#  define LZO_BYTE(x)       ((unsigned char) (x))

#define LZO_SIZE(bits)      (1u << (bits))
#define LZO_MASK(bits)      (LZO_SIZE(bits) - 1)

#define LZO_LSIZE(bits)     (1ul << (bits))
#define LZO_LMASK(bits)     (LZO_LSIZE(bits) - 1)

#define LZO_USIZE(bits)     ((lzo_uint) 1 << (bits))
#define LZO_UMASK(bits)     (LZO_USIZE(bits) - 1)

#if !defined(DMUL)
#  define DMUL(a,b) ((lzo_xint) ((a) * (b)))
#endif

#if !defined(LZO_CFG_NO_UNALIGNED)
#if (LZO_ARCH_AMD64 || LZO_ARCH_I386)
#  if (LZO_SIZEOF_SHORT == 2)
#    define LZO_UNALIGNED_OK_2
#  endif
#  if (LZO_SIZEOF_INT == 4)
#    define LZO_UNALIGNED_OK_4
#  endif
#endif
#endif

#if defined(LZO_UNALIGNED_OK_2)
  LZO_COMPILE_TIME_ASSERT_HEADER(sizeof(short) == 2)
#endif
#if defined(LZO_UNALIGNED_OK_4)
  LZO_COMPILE_TIME_ASSERT_HEADER(sizeof(lzo_uint32) == 4)
#elif defined(LZO_ALIGNED_OK_4)
  LZO_COMPILE_TIME_ASSERT_HEADER(sizeof(lzo_uint32) == 4)
#endif

extern int __lzo_init_done;
extern const char __lzo_copyright[];
LZO_EXTERN(const lzo_bytep) lzo_copyright(void);

// Start of pointer alignment definitions.

#if !defined(lzo_uintptr_t)
#    define lzo_uintptr_t   acc_uintptr_t
#    ifdef __ACC_INTPTR_T_IS_POINTER
#      define __LZO_UINTPTR_T_IS_POINTER 1
#    endif
#endif

#define PTR(a)              ((lzo_uintptr_t) (a))
#define PTR_LINEAR(a)       PTR(a)
#define PTR_ALIGNED_4(a)    ((PTR_LINEAR(a) & 3) == 0)
#define PTR_ALIGNED_8(a)    ((PTR_LINEAR(a) & 7) == 0)
#define PTR_ALIGNED2_4(a,b) (((PTR_LINEAR(a) | PTR_LINEAR(b)) & 3) == 0)
#define PTR_ALIGNED2_8(a,b) (((PTR_LINEAR(a) | PTR_LINEAR(b)) & 7) == 0)

#define PTR_LT(a,b)         (PTR(a) < PTR(b))
#define PTR_GE(a,b)         (PTR(a) >= PTR(b))
#define PTR_DIFF(a,b)       (PTR(a) - PTR(b))
#define pd(a,b)             ((lzo_uint) ((a)-(b)))

LZO_EXTERN(lzo_uintptr_t)
__lzo_ptr_linear(const lzo_voidp ptr);

typedef union
{
    char            a_char;
    unsigned char   a_uchar;
    short           a_short;
    unsigned short  a_ushort;
    int             a_int;
    unsigned int    a_uint;
    long            a_long;
    unsigned long   a_ulong;
    lzo_int         a_lzo_int;
    lzo_uint        a_lzo_uint;
    lzo_int32       a_lzo_int32;
    lzo_uint32      a_lzo_uint32;
    ptrdiff_t       a_ptrdiff_t;
    lzo_uintptr_t   a_lzo_uintptr_t;
    lzo_voidp       a_lzo_voidp;
    void *          a_void_p;
    lzo_bytep       a_lzo_bytep;
    lzo_bytepp      a_lzo_bytepp;
    lzo_uintp       a_lzo_uintp;
    lzo_uint *      a_lzo_uint_p;
    lzo_uint32p     a_lzo_uint32p;
    lzo_uint32 *    a_lzo_uint32_p;
    unsigned char * a_uchar_p;
    char *          a_char_p;
}
lzo_full_align_t;

// End of pointer alignment definitions.

#  define lzo_dict_t    const lzo_bytep
#  define lzo_dict_p    lzo_dict_t *

// End of configuration.

/* If you use the LZO library in a product, I would appreciate that you
 * keep this copyright string in the executable of your product.
 */
const char __lzo_copyright[] = LZO_VERSION_STRING;

LZO_PUBLIC(const lzo_bytep)
lzo_copyright(void)
{
    return (const lzo_bytep) __lzo_copyright;
}

LZO_PUBLIC(int)
_lzo_config_check(void)
{
    lzo_bool r = 1;
    union { unsigned char c[2*sizeof(lzo_xint)]; lzo_xint l[2]; } u;
    lzo_uintptr_t p;

#if !defined(LZO_CFG_NO_CONFIG_CHECK)
#if defined(LZO_ABI_BIG_ENDIAN)
    u.l[0] = u.l[1] = 0; u.c[sizeof(lzo_xint) - 1] = 128;
    r &= (u.l[0] == 128);
#endif
#if defined(LZO_ABI_LITTLE_ENDIAN)
    u.l[0] = u.l[1] = 0; u.c[0] = 128;
    r &= (u.l[0] == 128);
#endif
#if defined(LZO_UNALIGNED_OK_2)
    p = (lzo_uintptr_t) (const lzo_voidp) &u.c[0];
    u.l[0] = u.l[1] = 0;
    r &= ((* (const lzo_ushortp) (p+1)) == 0);
#endif
#if defined(LZO_UNALIGNED_OK_4)
    p = (lzo_uintptr_t) (const lzo_voidp) &u.c[0];
    u.l[0] = u.l[1] = 0;
    r &= ((* (const lzo_uint32p) (p+1)) == 0);
#endif
#endif

    LZO_UNUSED(u); LZO_UNUSED(p);
    return r == 1 ? LZO_E_OK : LZO_E_ERROR;
}

int __lzo_init_done = 0;

LZO_PUBLIC(int)
__lzo_init_v2(unsigned v, int s1, int s2, int s3, int s4, int s5,
                          int s6, int s7, int s8)
{
    int r;

    __lzo_init_done = 1;

    if (v == 0)
        return LZO_E_ERROR;

    r = (s1 == -1 || s1 == (int) sizeof(short)) &&
        (s2 == -1 || s2 == (int) sizeof(int)) &&
        (s3 == -1 || s3 == (int) sizeof(long)) &&
        (s4 == -1 || s4 == (int) sizeof(lzo_uint32)) &&
        (s5 == -1 || s5 == (int) sizeof(lzo_uint)) &&
        (s6 == -1 || s6 == (int) lzo_sizeof_dict_t) &&
        (s7 == -1 || s7 == (int) sizeof(char *)) &&
        (s8 == -1 || s8 == (int) sizeof(lzo_voidp));
    if (!r)
        return LZO_E_ERROR;

    r = _lzo_config_check();
    if (r != LZO_E_OK)
        return r;

    return r;
}

#define D_BITS          14
#define D_INDEX1(d,p)       d = DM(DMUL(0x21,DX3(p,5,5,6)) >> 5)
#define D_INDEX2(d,p)       d = (d & (D_MASK & 0x7ff)) ^ (D_HIGH | 0x1f)

// Start of LZO1X.

#if !defined(LZO1X) && !defined(LZO1Y) && !defined(LZO1Z)
#  define LZO1X
#endif

#define M1_MAX_OFFSET   0x0400
#ifndef M2_MAX_OFFSET
#define M2_MAX_OFFSET   0x0800
#endif
#define M3_MAX_OFFSET   0x4000
#define M4_MAX_OFFSET   0xbfff

#define MX_MAX_OFFSET   (M1_MAX_OFFSET + M2_MAX_OFFSET)

#define M1_MIN_LEN      2
#define M1_MAX_LEN      2
#define M2_MIN_LEN      3
#ifndef M2_MAX_LEN
#define M2_MAX_LEN      8
#endif
#define M3_MIN_LEN      3
#define M3_MAX_LEN      33
#define M4_MIN_LEN      3
#define M4_MAX_LEN      9

#define M1_MARKER       0
#define M2_MARKER       64
#define M3_MARKER       32
#define M4_MARKER       16

#ifndef MIN_LOOKAHEAD
#define MIN_LOOKAHEAD       (M2_MAX_LEN + 1)
#endif

#define DL_MIN_LEN          M2_MIN_LEN

// Start of dictionary macros.

#if !defined(D_BITS) && defined(DBITS)
#  define D_BITS        DBITS
#endif
#if !defined(D_BITS)
#  error "D_BITS is not defined"
#endif
#if (D_BITS < 16)
#  define D_SIZE        LZO_SIZE(D_BITS)
#  define D_MASK        LZO_MASK(D_BITS)
#else
#  define D_SIZE        LZO_USIZE(D_BITS)
#  define D_MASK        LZO_UMASK(D_BITS)
#endif
#define D_HIGH          ((D_MASK >> 1) + 1)

#if !defined(DD_BITS)
#  define DD_BITS       0
#endif
#define DD_SIZE         LZO_SIZE(DD_BITS)
#define DD_MASK         LZO_MASK(DD_BITS)

#if !defined(DL_BITS)
#  define DL_BITS       (D_BITS - DD_BITS)
#endif
#if (DL_BITS < 16)
#  define DL_SIZE       LZO_SIZE(DL_BITS)
#  define DL_MASK       LZO_MASK(DL_BITS)
#else
#  define DL_SIZE       LZO_USIZE(DL_BITS)
#  define DL_MASK       LZO_UMASK(DL_BITS)
#endif

#if (D_BITS != DL_BITS + DD_BITS)
#  error "D_BITS does not match"
#endif
#if (D_BITS < 8 || D_BITS > 18)
#  error "invalid D_BITS"
#endif
#if (DL_BITS < 8 || DL_BITS > 20)
#  error "invalid DL_BITS"
#endif
#if (DD_BITS < 0 || DD_BITS > 6)
#  error "invalid DD_BITS"
#endif

#if !defined(DL_MIN_LEN)
#  define DL_MIN_LEN    3
#endif
#if !defined(DL_SHIFT)
#  define DL_SHIFT      ((DL_BITS + (DL_MIN_LEN - 1)) / DL_MIN_LEN)
#endif

#undef DM
#undef DX

#if (DL_MIN_LEN == 3)
#  define _DV2_A(p,shift1,shift2) \
        (((( (lzo_xint)((p)[0]) << shift1) ^ (p)[1]) << shift2) ^ (p)[2])
#  define _DV2_B(p,shift1,shift2) \
        (((( (lzo_xint)((p)[2]) << shift1) ^ (p)[1]) << shift2) ^ (p)[0])
#  define _DV3_B(p,shift1,shift2,shift3) \
        ((_DV2_B((p)+1,shift1,shift2) << (shift3)) ^ (p)[0])
#elif (DL_MIN_LEN == 2)
#  define _DV2_A(p,shift1,shift2) \
        (( (lzo_xint)(p[0]) << shift1) ^ p[1])
#  define _DV2_B(p,shift1,shift2) \
        (( (lzo_xint)(p[1]) << shift1) ^ p[2])
#else
#  error "invalid DL_MIN_LEN"
#endif
#define _DV_A(p,shift)      _DV2_A(p,shift,shift)
#define _DV_B(p,shift)      _DV2_B(p,shift,shift)
#define DA2(p,s1,s2) \
        (((((lzo_xint)((p)[2]) << (s2)) + (p)[1]) << (s1)) + (p)[0])
#define DS2(p,s1,s2) \
        (((((lzo_xint)((p)[2]) << (s2)) - (p)[1]) << (s1)) - (p)[0])
#define DX2(p,s1,s2) \
        (((((lzo_xint)((p)[2]) << (s2)) ^ (p)[1]) << (s1)) ^ (p)[0])
#define DA3(p,s1,s2,s3) ((DA2((p)+1,s2,s3) << (s1)) + (p)[0])
#define DS3(p,s1,s2,s3) ((DS2((p)+1,s2,s3) << (s1)) - (p)[0])
#define DX3(p,s1,s2,s3) ((DX2((p)+1,s2,s3) << (s1)) ^ (p)[0])
#define DMS(v,s)        ((lzo_uint) (((v) & (D_MASK >> (s))) << (s)))
#define DM(v)           DMS(v,0)

#  define DENTRY(p,in)                          (p)
#  define GINDEX(m_pos,m_off,dict,dindex,in)    m_pos = dict[dindex]

#if (DD_BITS == 0)

#  define UPDATE_I(dict,drun,index,p,in)    dict[index] = DENTRY(p,in)
#  define UPDATE_P(ptr,drun,p,in)           (ptr)[0] = DENTRY(p,in)

#else

#  define UPDATE_I(dict,drun,index,p,in)    \
        dict[ (index) + drun++ ] = DENTRY(p,in); drun &= DD_MASK
#  define UPDATE_P(ptr,drun,p,in)   \
        (ptr) [ drun++ ] = DENTRY(p,in); drun &= DD_MASK

#endif

#define LZO_CHECK_MPOS_DET(m_pos,m_off,in,ip,max_offset) \
        (m_pos == NULL || (m_off = pd(ip, m_pos)) > max_offset)

#define LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,max_offset) \
    ( \
        m_pos = ip - (lzo_uint) PTR_DIFF(ip,m_pos), \
        PTR_LT(m_pos,in) || \
        (m_off = (lzo_uint) PTR_DIFF(ip,m_pos)) <= 0 || \
         m_off > max_offset )

// End of dictionary macros.

// End of LZO1X.

static lzo_uint
_lzo1x_1_do_compress(const lzo_bytep in, lzo_uint  in_len,
                     lzo_bytep out, lzo_uintp out_len,
                     lzo_voidp wrkmem)
{
    register const lzo_bytep ip;
    lzo_bytep op;
    const lzo_bytep const in_end = in + in_len;
    const lzo_bytep const ip_end = in + in_len - M2_MAX_LEN - 5;
    const lzo_bytep ii;
    lzo_dict_p const dict = (lzo_dict_p) wrkmem;

    op = out;
    ip = in;
    ii = ip;

    ip += 4;
    for (;;)
    {
        register const lzo_bytep m_pos;
        lzo_uint m_off;
        lzo_uint m_len;
        lzo_uint dindex;

        D_INDEX1(dindex,ip);
        GINDEX(m_pos,m_off,dict,dindex,in);
        if (LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,M4_MAX_OFFSET))
            goto literal;
        if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3])
            goto try_match;
        D_INDEX2(dindex,ip);
        GINDEX(m_pos,m_off,dict,dindex,in);
        if (LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,M4_MAX_OFFSET))
            goto literal;
        if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3])
            goto try_match;
        goto literal;

try_match:
#if defined(LZO_UNALIGNED_OK_2)
        if (* (const lzo_ushortp) m_pos != * (const lzo_ushortp) ip)
#else
        if (m_pos[0] != ip[0] || m_pos[1] != ip[1])
#endif
        {
        }
        else
        {
            if __lzo_likely(m_pos[2] == ip[2])
            {
                    goto match;
            }
        }

literal:
        UPDATE_I(dict,0,dindex,ip,in);
        ++ip;
        if __lzo_unlikely(ip >= ip_end)
            break;
        continue;

match:
        UPDATE_I(dict,0,dindex,ip,in);
        if (pd(ip,ii) > 0)
        {
            register lzo_uint t = pd(ip,ii);

            if (t <= 3)
            {
                assert(op - 2 > out);
                op[-2] |= LZO_BYTE(t);
            }
            else if (t <= 18)
                *op++ = LZO_BYTE(t - 3);
            else
            {
                register lzo_uint tt = t - 18;

                *op++ = 0;
                while (tt > 255)
                {
                    tt -= 255;
                    *op++ = 0;
                }
                assert(tt > 0);
                *op++ = LZO_BYTE(tt);
            }
            do *op++ = *ii++; while (--t > 0);
        }

        assert(ii == ip);
        ip += 3;
        if (m_pos[3] != *ip++ || m_pos[4] != *ip++ || m_pos[5] != *ip++ ||
            m_pos[6] != *ip++ || m_pos[7] != *ip++ || m_pos[8] != *ip++
#ifdef LZO1Y
            || m_pos[ 9] != *ip++ || m_pos[10] != *ip++ || m_pos[11] != *ip++
            || m_pos[12] != *ip++ || m_pos[13] != *ip++ || m_pos[14] != *ip++
#endif
           )
        {
            --ip;
            m_len = pd(ip, ii);
            assert(m_len >= 3); assert(m_len <= M2_MAX_LEN);

            if (m_off <= M2_MAX_OFFSET)
            {
                m_off -= 1;
#if defined(LZO1X)
                *op++ = LZO_BYTE(((m_len - 1) << 5) | ((m_off & 7) << 2));
                *op++ = LZO_BYTE(m_off >> 3);
#elif defined(LZO1Y)
                *op++ = LZO_BYTE(((m_len + 1) << 4) | ((m_off & 3) << 2));
                *op++ = LZO_BYTE(m_off >> 2);
#endif
            }
            else if (m_off <= M3_MAX_OFFSET)
            {
                m_off -= 1;
                *op++ = LZO_BYTE(M3_MARKER | (m_len - 2));
                goto m3_m4_offset;
            }
            else
#if defined(LZO1X)
            {
                m_off -= 0x4000;
                assert(m_off > 0); assert(m_off <= 0x7fff);
                *op++ = LZO_BYTE(M4_MARKER |
                                 ((m_off & 0x4000) >> 11) | (m_len - 2));
                goto m3_m4_offset;
            }
#elif defined(LZO1Y)
                goto m4_match;
#endif
        }
        else
        {
            {
                const lzo_bytep end = in_end;
                const lzo_bytep m = m_pos + M2_MAX_LEN + 1;
                while (ip < end && *m == *ip)
                    m++, ip++;
                m_len = pd(ip, ii);
            }
            assert(m_len > M2_MAX_LEN);

            if (m_off <= M3_MAX_OFFSET)
            {
                m_off -= 1;
                if (m_len <= 33)
                    *op++ = LZO_BYTE(M3_MARKER | (m_len - 2));
                else
                {
                    m_len -= 33;
                    *op++ = M3_MARKER | 0;
                    goto m3_m4_len;
                }
            }
            else
            {
#if defined(LZO1Y)
m4_match:
#endif
                m_off -= 0x4000;
                assert(m_off > 0); assert(m_off <= 0x7fff);
                if (m_len <= M4_MAX_LEN)
                    *op++ = LZO_BYTE(M4_MARKER |
                                     ((m_off & 0x4000) >> 11) | (m_len - 2));
                else
                {
                    m_len -= M4_MAX_LEN;
                    *op++ = LZO_BYTE(M4_MARKER | ((m_off & 0x4000) >> 11));
m3_m4_len:
                    while (m_len > 255)
                    {
                        m_len -= 255;
                        *op++ = 0;
                    }
                    assert(m_len > 0);
                    *op++ = LZO_BYTE(m_len);
                }
            }

m3_m4_offset:
            *op++ = LZO_BYTE((m_off & 63) << 2);
            *op++ = LZO_BYTE(m_off >> 6);
        }

        ii = ip;
        if __lzo_unlikely(ip >= ip_end)
            break;
    }

    *out_len = pd(op, out);
    return pd(in_end,ii);
}

LZO_PUBLIC(int)
lzo1x_1_compress(const lzo_bytep in, lzo_uint  in_len,
                 lzo_bytep out, lzo_uintp out_len,
                 lzo_voidp wrkmem)
{
    lzo_bytep op = out;
    lzo_uint t;

    if __lzo_unlikely(in_len <= M2_MAX_LEN + 5)
        t = in_len;
    else
    {
        t = _lzo1x_1_do_compress(in,in_len,op,out_len,wrkmem);
        op += *out_len;
    }

    if (t > 0)
    {
        const lzo_bytep ii = in + in_len - t;

        if (op == out && t <= 238)
            *op++ = LZO_BYTE(17 + t);
        else if (t <= 3)
            op[-2] |= LZO_BYTE(t);
        else if (t <= 18)
            *op++ = LZO_BYTE(t - 3);
        else
        {
            lzo_uint tt = t - 18;

            *op++ = 0;
            while (tt > 255)
            {
                tt -= 255;
                *op++ = 0;
            }
            assert(tt > 0);
            *op++ = LZO_BYTE(tt);
        }
        do *op++ = *ii++; while (--t > 0);
    }

    *op++ = M4_MARKER | 1;
    *op++ = 0;
    *op++ = 0;

    *out_len = pd(op, out);
    return LZO_E_OK;
}

#undef __COPY4
#define __COPY4(dst,src)    * (lzo_uint32p)(dst) = * (const lzo_uint32p)(src)

#undef COPY4
#if defined(LZO_UNALIGNED_OK_4)
#  define COPY4(dst,src)    __COPY4(dst,src)
#elif defined(LZO_ALIGNED_OK_4)
#  define COPY4(dst,src)    __COPY4((lzo_uintptr_t)(dst),(lzo_uintptr_t)(src))
#endif

LZO_PUBLIC(int)
lzo1x_decompress(const lzo_bytep in, lzo_uint  in_len,
                 lzo_bytep out, lzo_uintp out_len,
                 lzo_voidp wrkmem)
{
    register lzo_bytep op;
    register const lzo_bytep ip;
    register lzo_uint t;
    register const lzo_bytep m_pos;

    const lzo_bytep const ip_end = in + in_len;
#if defined(LZO1Z)
    lzo_uint last_m_off = 0;
#endif

    LZO_UNUSED(wrkmem);

    *out_len = 0;

    op = out;
    ip = in;

    if (*ip > 17)
    {
        t = *ip++ - 17;
        if (t < 4)
            goto match_next;
        assert(t > 0);
        do *op++ = *ip++; while (--t > 0);
        goto first_literal_run;
    }

    while (true)
    {
        t = *ip++;
        if (t >= 16)
            goto match;
        if (t == 0)
        {
            while (*ip == 0)
            {
                t += 255;
                ip++;
            }
            t += 15 + *ip++;
        }
        assert(t > 0);
#if defined(LZO_UNALIGNED_OK_4) || defined(LZO_ALIGNED_OK_4)
#if !defined(LZO_UNALIGNED_OK_4)
        if (PTR_ALIGNED2_4(op,ip))
        {
#endif
        COPY4(op,ip);
        op += 4; ip += 4;
        if (--t > 0)
        {
            if (t >= 4)
            {
                do {
                    COPY4(op,ip);
                    op += 4; ip += 4; t -= 4;
                } while (t >= 4);
                if (t > 0) do *op++ = *ip++; while (--t > 0);
            }
            else
                do *op++ = *ip++; while (--t > 0);
        }
#if !defined(LZO_UNALIGNED_OK_4)
        }
        else
#endif
#endif
#if !defined(LZO_UNALIGNED_OK_4)
        {
            *op++ = *ip++; *op++ = *ip++; *op++ = *ip++;
            do *op++ = *ip++; while (--t > 0);
        }
#endif

first_literal_run:

        t = *ip++;
        if (t >= 16)
            goto match;
#if defined(LZO1Z)
        t = (1 + M2_MAX_OFFSET) + (t << 6) + (*ip++ >> 2);
        m_pos = op - t;
        last_m_off = t;
#else
        m_pos = op - (1 + M2_MAX_OFFSET);
        m_pos -= t >> 2;
        m_pos -= *ip++ << 2;
#endif
        *op++ = *m_pos++; *op++ = *m_pos++; *op++ = *m_pos;
        goto match_done;

        do {
match:
            if (t >= 64)
            {
#if defined(LZO1X)
                m_pos = op - 1;
                m_pos -= (t >> 2) & 7;
                m_pos -= *ip++ << 3;
                t = (t >> 5) - 1;
#elif defined(LZO1Y)
                m_pos = op - 1;
                m_pos -= (t >> 2) & 3;
                m_pos -= *ip++ << 2;
                t = (t >> 4) - 3;
#elif defined(LZO1Z)
                {
                    lzo_uint off = t & 0x1f;
                    m_pos = op;
                    if (off >= 0x1c)
                    {
                        assert(last_m_off > 0);
                        m_pos -= last_m_off;
                    }
                    else
                    {
                        off = 1 + (off << 6) + (*ip++ >> 2);
                        m_pos -= off;
                        last_m_off = off;
                    }
                }
                t = (t >> 5) - 1;
#endif
                assert(t > 0);
                goto copy_match;
            }
            else if (t >= 32)
            {
                t &= 31;
                if (t == 0)
                {
                    while (*ip == 0)
                    {
                        t += 255;
                        ip++;
                    }
                    t += 31 + *ip++;
                }
#if defined(LZO1Z)
                {
                    lzo_uint off = 1 + (ip[0] << 6) + (ip[1] >> 2);
                    m_pos = op - off;
                    last_m_off = off;
                }
#elif defined(LZO_UNALIGNED_OK_2) && defined(LZO_ABI_LITTLE_ENDIAN)
                m_pos = op - 1;
                m_pos -= (* (const lzo_ushortp) ip) >> 2;
#else
                m_pos = op - 1;
                m_pos -= (ip[0] >> 2) + (ip[1] << 6);
#endif
                ip += 2;
            }
            else if (t >= 16)
            {
                m_pos = op;
                m_pos -= (t & 8) << 11;
                t &= 7;
                if (t == 0)
                {
                    while (*ip == 0)
                    {
                        t += 255;
                        ip++;
                    }
                    t += 7 + *ip++;
                }
#if defined(LZO1Z)
                m_pos -= (ip[0] << 6) + (ip[1] >> 2);
#elif defined(LZO_UNALIGNED_OK_2) && defined(LZO_ABI_LITTLE_ENDIAN)
                m_pos -= (* (const lzo_ushortp) ip) >> 2;
#else
                m_pos -= (ip[0] >> 2) + (ip[1] << 6);
#endif
                ip += 2;
                if (m_pos == op)
                    goto eof_found;
                m_pos -= 0x4000;
#if defined(LZO1Z)
                last_m_off = pd((const lzo_bytep)op, m_pos);
#endif
            }
            else
            {
#if defined(LZO1Z)
                t = 1 + (t << 6) + (*ip++ >> 2);
                m_pos = op - t;
                last_m_off = t;
#else
                m_pos = op - 1;
                m_pos -= t >> 2;
                m_pos -= *ip++ << 2;
#endif
                *op++ = *m_pos++; *op++ = *m_pos;
                goto match_done;
            }

            assert(t > 0);
#if defined(LZO_UNALIGNED_OK_4) || defined(LZO_ALIGNED_OK_4)
#if !defined(LZO_UNALIGNED_OK_4)
            if (t >= 2 * 4 - (3 - 1) && PTR_ALIGNED2_4(op,m_pos))
            {
                assert((op - m_pos) >= 4);
#else
            if (t >= 2 * 4 - (3 - 1) && (op - m_pos) >= 4)
            {
#endif
                COPY4(op,m_pos);
                op += 4; m_pos += 4; t -= 4 - (3 - 1);
                do {
                    COPY4(op,m_pos);
                    op += 4; m_pos += 4; t -= 4;
                } while (t >= 4);
                if (t > 0) do *op++ = *m_pos++; while (--t > 0);
            }
            else
#endif
            {
copy_match:
                *op++ = *m_pos++; *op++ = *m_pos++;
                do *op++ = *m_pos++; while (--t > 0);
            }

match_done:
#if defined(LZO1Z)
            t = ip[-1] & 3;
#else
            t = ip[-2] & 3;
#endif
            if (t == 0)
                break;

match_next:
            assert(t > 0); assert(t < 4);
            *op++ = *ip++;
            if (t > 1) { *op++ = *ip++; if (t > 2) { *op++ = *ip++; } }
            t = *ip++;
        } while (true);
    }

eof_found:
    assert(t == 1);
    *out_len = pd(op, out);
    return (ip == ip_end ? LZO_E_OK :
           (ip < ip_end  ? LZO_E_INPUT_NOT_CONSUMED : LZO_E_INPUT_OVERRUN));
}
