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

#include "lzo.hh"
#include "inline.hh"
#include "likely.hh"
#include "openmsx.hh"
#include "build-info.hh"

#include <cassert>
#include <climits>
#include <cstddef>
#include <cstring>

// Start of configuration.

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
LZO_COMPILE_TIME_ASSERT_HEADER(sizeof(lzo_uintptr_t) >= sizeof(lzo_voidp))

#  define LZO_BYTE(x)       ((unsigned char) (x))

#define LZO_SIZE(bits)      (1u << (bits))
#define LZO_MASK(bits)      (LZO_SIZE(bits) - 1)

#  define lzo_dict_t    const lzo_bytep
#  define lzo_dict_p    lzo_dict_t *

// End of configuration.

namespace openmsx {

/* If you use the LZO library in a product, I would appreciate that you
 * keep this copyright string in the executable of your product.
 */
const char __lzo_copyright[] = LZO_VERSION_STRING;

const lzo_bytep lzo_copyright(void)
{
	return (const lzo_bytep) __lzo_copyright;
}

int _lzo_config_check(void)
{
	lzo_bool r = 1;
	union { unsigned char c[2*sizeof(lzo_xint)]; lzo_xint l[2]; } u;
	lzo_uintptr_t p;

	if (OPENMSX_BIGENDIAN) {
		u.l[0] = u.l[1] = 0; u.c[sizeof(lzo_xint) - 1] = 128;
		r &= (u.l[0] == 128);
	} else {
		u.l[0] = u.l[1] = 0; u.c[0] = 128;
		r &= (u.l[0] == 128);
	}
	if (OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		p = (lzo_uintptr_t) (const lzo_voidp) &u.c[0];
		u.l[0] = u.l[1] = 0;
		r &= ((* (const lzo_ushortp) (p+1)) == 0);
		r &= ((* (const lzo_uint32p) (p+1)) == 0);
	}

	(void)u; (void)p;
	return r == 1 ? LZO_E_OK : LZO_E_ERROR;
}

int __lzo_init_done = 0;

int __lzo_init_v2(unsigned v, int s1, int s2, int s3, int s4, int s5,
                  int s6, int s7, int s8)
{
	int r;

	__lzo_init_done = 1;

	if (v == 0) {
		return LZO_E_ERROR;
	}

	r = (s1 == -1 || s1 == (int) sizeof(short)) &&
		(s2 == -1 || s2 == (int) sizeof(int)) &&
		(s3 == -1 || s3 == (int) sizeof(long)) &&
		(s4 == -1 || s4 == (int) sizeof(lzo_uint32)) &&
		(s5 == -1 || s5 == (int) sizeof(lzo_uint)) &&
		(s6 == -1 || s6 == (int) lzo_sizeof_dict_t) &&
		(s7 == -1 || s7 == (int) sizeof(char *)) &&
		(s8 == -1 || s8 == (int) sizeof(lzo_voidp));
	if (!r) {
		return LZO_E_ERROR;
	}

	r = _lzo_config_check();
	if (r != LZO_E_OK) {
		return r;
	}

	return r;
}

#define DMUL(a,b) ((lzo_xint) ((a) * (b)))
#define D_INDEX1(d,p)       d = DM(DMUL(0x21,DX3(p,5,5,6)) >> 5)
#define D_INDEX2(d,p)       d = (d & (D_MASK & 0x7ff)) ^ (D_HIGH | 0x1f)

// Start of LZO1X.

#define M2_MAX_OFFSET   0x0800
#define M3_MAX_OFFSET   0x4000
#define M4_MAX_OFFSET   0xbfff

#define M2_MAX_LEN      8
#define M4_MAX_LEN      9

#define M3_MARKER       32
#define M4_MARKER       16

// Start of dictionary macros.

#define D_BITS          14
#  define D_MASK        LZO_MASK(D_BITS)
#define D_HIGH          ((D_MASK >> 1) + 1)

#define DX2(p,s1,s2) \
        (((((lzo_xint)((p)[2]) << (s2)) ^ (p)[1]) << (s1)) ^ (p)[0])
#define DX3(p,s1,s2,s3) ((DX2((p)+1,s2,s3) << (s1)) ^ (p)[0])
#define DM(v)           ((lzo_uint) ((v) & D_MASK))

#define PTR(a)              ((lzo_uintptr_t) (a))
#define LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,max_offset) \
    ( \
        m_pos = ip - (lzo_uint) (PTR(ip) - PTR(m_pos)), \
        (PTR(m_pos) < PTR(in)) || \
        (m_off = (lzo_uint) (PTR(ip) - PTR(m_pos))) <= 0 || \
         m_off > max_offset )

// End of dictionary macros.

// End of LZO1X.

static lzo_uint
_lzo1x_1_do_compress(const lzo_bytep in, lzo_uint in_len,
                     lzo_bytep out, lzo_uintp out_len,
                     lzo_voidp wrkmem)
{
	const lzo_bytep ip;
	lzo_bytep op;
	const lzo_bytep const in_end = in + in_len;
	const lzo_bytep const ip_end = in + in_len - M2_MAX_LEN - 5;
	const lzo_bytep ii;
	lzo_dict_p const dict = (lzo_dict_p) wrkmem;

	op = out;
	ip = in;
	ii = ip;

	ip += 4;
	for (;;) {
		const lzo_bytep m_pos;
		lzo_uint m_off;
		lzo_uint m_len;
		lzo_uint dindex;

		D_INDEX1(dindex,ip);
		m_pos = dict[dindex];
		if (LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,M4_MAX_OFFSET)) {
			goto literal;
		}
		if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3]) {
			goto try_match;
		}
		D_INDEX2(dindex,ip);
		m_pos = dict[dindex];
		if (LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,M4_MAX_OFFSET)) {
			goto literal;
		}
		if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3]) {
			goto try_match;
		}
		goto literal;

try_match:
		if (OPENMSX_UNALIGNED_MEMORY_ACCESS
			? (*(const lzo_ushortp)m_pos != *(const lzo_ushortp)ip)
			: (m_pos[0] != ip[0] || m_pos[1] != ip[1])
		) {
		} else if (likely(m_pos[2] == ip[2])) {
			goto match;
		}

literal:
		dict[dindex] = ip;
		++ip;
		if (unlikely(ip >= ip_end)) {
			break;
		}
		continue;

match:
		dict[dindex] = ip;
		if (ip > ii) {
			lzo_uint t = ip - ii;

			if (t <= 3) {
				assert(op - 2 > out);
				op[-2] |= LZO_BYTE(t);
			} else if (t <= 18) {
				*op++ = LZO_BYTE(t - 3);
			} else {
				lzo_uint tt = t - 18;

				*op++ = 0;
				while (tt > 255) {
					tt -= 255;
					*op++ = 0;
				}
				assert(tt > 0);
				*op++ = LZO_BYTE(tt);
			}
			do { *op++ = *ii++; } while (--t > 0);
		}

		assert(ii == ip);
		ip += 3;
		if (m_pos[3] != *ip++ || m_pos[4] != *ip++ || m_pos[5] != *ip++ ||
			m_pos[6] != *ip++ || m_pos[7] != *ip++ || m_pos[8] != *ip++
		) {
			--ip;
			m_len = ip - ii;
			assert(m_len >= 3); assert(m_len <= M2_MAX_LEN);

			if (m_off <= M2_MAX_OFFSET) {
				m_off -= 1;
				*op++ = LZO_BYTE(((m_len - 1) << 5) | ((m_off & 7) << 2));
				*op++ = LZO_BYTE(m_off >> 3);
			} else if (m_off <= M3_MAX_OFFSET) {
				m_off -= 1;
				*op++ = LZO_BYTE(M3_MARKER | (m_len - 2));
				goto m3_m4_offset;
			} else {
				m_off -= 0x4000;
				assert(m_off > 0); assert(m_off <= 0x7fff);
				*op++ = LZO_BYTE(M4_MARKER |
								((m_off & 0x4000) >> 11) | (m_len - 2));
				goto m3_m4_offset;
			}
		} else {
			{
				const lzo_bytep end = in_end;
				const lzo_bytep m = m_pos + M2_MAX_LEN + 1;
				while (ip < end && *m == *ip) {
					m++, ip++;
				}
				m_len = ip - ii;
			}
			assert(m_len > M2_MAX_LEN);

			if (m_off <= M3_MAX_OFFSET) {
				m_off -= 1;
				if (m_len <= 33) {
					*op++ = LZO_BYTE(M3_MARKER | (m_len - 2));
				} else {
					m_len -= 33;
					*op++ = M3_MARKER | 0;
					goto m3_m4_len;
				}
			} else {
				m_off -= 0x4000;
				assert(m_off > 0); assert(m_off <= 0x7fff);
				if (m_len <= M4_MAX_LEN) {
					*op++ = LZO_BYTE(M4_MARKER |
									((m_off & 0x4000) >> 11) | (m_len - 2));
				} else {
					m_len -= M4_MAX_LEN;
					*op++ = LZO_BYTE(M4_MARKER | ((m_off & 0x4000) >> 11));
m3_m4_len:
					while (m_len > 255) {
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
		if (unlikely(ip >= ip_end)) {
			break;
		}
	}

	*out_len = op - out;
	return in_end - ii;
}

void lzo1x_1_compress(const lzo_bytep in, lzo_uint in_len,
                      lzo_bytep out, lzo_uintp out_len,
                      lzo_voidp wrkmem)
{
	lzo_bytep op = out;
	lzo_uint t;

	if (unlikely(in_len <= M2_MAX_LEN + 5)) {
		t = in_len;
	} else {
		t = _lzo1x_1_do_compress(in,in_len,op,out_len,wrkmem);
		op += *out_len;
	}

	if (t > 0) {
		const lzo_bytep ii = in + in_len - t;

		if (op == out && t <= 238) {
			*op++ = LZO_BYTE(17 + t);
		} else if (t <= 3) {
			op[-2] |= LZO_BYTE(t);
		} else if (t <= 18) {
			*op++ = LZO_BYTE(t - 3);
		} else {
			lzo_uint tt = t - 18;

			*op++ = 0;
			while (tt > 255) {
				tt -= 255;
				*op++ = 0;
			}
			assert(tt > 0);
			*op++ = LZO_BYTE(tt);
		}
		do { *op++ = *ii++; } while (--t > 0);
	}

	*op++ = M4_MARKER | 1;
	*op++ = 0;
	*op++ = 0;

	*out_len = op - out;
}

// TODO: This function was copy-pasted from CPUCore.cc.
static ALWAYS_INLINE unsigned read16LE(const byte* p)
{
	if (OPENMSX_BIGENDIAN || !OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		return p[0] + 256 * p[1];
	} else {
		return *reinterpret_cast<const word*>(p);
	}
}

void lzo1x_decompress(
	const lzo_bytep __restrict src, lzo_uint __restrict src_len,
	lzo_bytep __restrict dst, lzo_uintp __restrict dst_len
) {
	lzo_bytep op;
	const lzo_bytep ip;
	lzo_uint t;
	const lzo_bytep m_pos;

	const lzo_bytep const src_end = src + src_len;

	*dst_len = 0;

	op = dst;
	ip = src;

	if (*ip > 17) {
		t = *ip++ - 17;
		if (t < 4) {
			goto match_next;
		}
		assert(t > 0);
		do { *op++ = *ip++; } while (--t > 0);
		goto first_literal_run;
	}

	while (true) {
		t = *ip++;
		if (t >= 16) {
			goto match;
		}
		if (t == 0) {
			while (*ip == 0) {
				t += 255;
				ip++;
			}
			t += 15 + *ip++;
		}
		assert(t > 0);
		memcpy(op, ip, t + 3);

first_literal_run:

		t = *ip++;
		if (t >= 16) {
			goto match;
		}
		m_pos = op - (1 + M2_MAX_OFFSET);
		m_pos -= t >> 2;
		m_pos -= *ip++ << 2;
		*op++ = *m_pos++; *op++ = *m_pos++; *op++ = *m_pos;
		goto match_done;

		do {
match:
			if (t >= 64) {
				m_pos = op - 1;
				m_pos -= (t >> 2) & 7;
				m_pos -= *ip++ << 3;
				t = (t >> 5) - 1;
				assert(t > 0);
				goto copy_match;
			} else if (t >= 32) {
				t &= 31;
				if (t == 0) {
					while (*ip == 0) {
						t += 255;
						ip++;
					}
					t += 31 + *ip++;
				}
				m_pos = op - 1 - (read16LE(ip) >> 2);
				ip += 2;
			} else if (t >= 16) {
				m_pos = op;
				m_pos -= (t & 8) << 11;
				t &= 7;
				if (t == 0) {
					while (*ip == 0) {
						t += 255;
						ip++;
					}
					t += 7 + *ip++;
				}
				m_pos -= read16LE(ip) >> 2;
				ip += 2;
				if (m_pos == op) {
					goto eof_found;
				}
				m_pos -= 0x4000;
			} else {
				m_pos = op - 1;
				m_pos -= t >> 2;
				m_pos -= *ip++ << 2;
				*op++ = *m_pos++; *op++ = *m_pos;
				goto match_done;
			}

			assert(t > 0);
copy_match:
			t += 2;
			do { *op++ = *m_pos++; } while (--t > 0);

match_done:
			t = ip[-2] & 3;
			if (t == 0) {
				break;
			}

match_next:
			assert(t > 0); assert(t < 4);
			*op++ = *ip++;
			if (t > 1) { *op++ = *ip++; if (t > 2) { *op++ = *ip++; } }
			t = *ip++;
		} while (true);
	}

eof_found:
	assert(t == 1);
	*dst_len = op - dst;
	assert(src == src_end); (void)src_end;
}

} // namespace openmsx
