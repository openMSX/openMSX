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
#include "build-info.hh"
#include <cassert>

namespace openmsx {

/* If you use the LZO library in a product, I would appreciate that you
 * keep this copyright string in the executable of your product.
 */
const char* lzo_copyright()
{
	static const char* const lzo_copyright = "2.03";
	return lzo_copyright;
}


static const unsigned M2_MAX_OFFSET = 0x0800;
static const unsigned M3_MAX_OFFSET = 0x4000;
static const unsigned M4_MAX_OFFSET = 0xbfff;
static const unsigned M2_MAX_LEN = 8;
static const unsigned M4_MAX_LEN = 9;
static const byte M3_MARKER = 32;
static const byte M4_MARKER = 16;

static const unsigned D_BITS = 14;
static const unsigned D_SIZE = (1 << D_BITS);
static const unsigned D_MASK = (D_SIZE - 1);
static const unsigned D_HIGH = ((D_MASK >> 1) + 1);


unsigned dict_hash(const byte* p)
{
	unsigned t = p[0] ^ (p[1] << 5) ^ (p[2] << 10) ^ (p[3] << 16);
	return (t + (t >> 5)) & D_MASK;
}

static unsigned _lzo1x_1_do_compress(const byte* in,  unsigned  in_len,
                                           byte* out, unsigned& out_len)
{
	const byte* dict[D_SIZE] = {}; // init with NULL pointers

	byte* op = out;
	const byte* ip = in;
	const byte* ii = ip;
	const byte* const in_end = in + in_len;
	const byte* const ip_end = in + in_len - M2_MAX_LEN - 5;

	ip += 4;
	while (true) {
		unsigned m_len;

		unsigned dindex = dict_hash(ip);
		const byte* m_pos = dict[dindex];
		unsigned m_off;
		if (m_pos < in
		|| (m_off = (ip - m_pos)) <= 0 || m_off > M4_MAX_OFFSET) {
			goto literal;
		}
		if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3]) {
			goto try_match;
		}
		dindex = (dindex & (D_MASK & 0x7ff)) ^ (D_HIGH | 0x1f);
		m_pos = dict[dindex];
		if (m_pos < in
		|| (m_off = (ip - m_pos)) <= 0 || m_off > M4_MAX_OFFSET) {
			goto literal;
		}
		if (m_off <= M2_MAX_OFFSET || m_pos[3] == ip[3]) {
			goto try_match;
		}
		goto literal;

try_match:
		if (OPENMSX_UNALIGNED_MEMORY_ACCESS
			? (*(word*)m_pos != *(word*)ip)
			: (m_pos[0] != ip[0] || m_pos[1] != ip[1])
		) {
			// nothing
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
			unsigned t = ip - ii;

			if (t <= 3) {
				assert(op - 2 > out);
				op[-2] |= byte(t);
			} else if (t <= 18) {
				*op++ = byte(t - 3);
			} else {
				unsigned tt = t - 18;

				*op++ = 0;
				while (tt > 255) {
					tt -= 255;
					*op++ = 0;
				}
				assert(tt > 0);
				*op++ = byte(tt);
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
				*op++ = byte(((m_len - 1) << 5) | ((m_off & 7) << 2));
				*op++ = byte(m_off >> 3);
			} else if (m_off <= M3_MAX_OFFSET) {
				m_off -= 1;
				*op++ = byte(M3_MARKER | (m_len - 2));
				goto m3_m4_offset;
			} else {
				m_off -= 0x4000;
				assert(m_off > 0); assert(m_off <= 0x7fff);
				*op++ = byte(M4_MARKER | ((m_off & 0x4000) >> 11) | (m_len - 2));
				goto m3_m4_offset;
			}
		} else {
			{
				const byte* end = in_end;
				const byte* m = m_pos + M2_MAX_LEN + 1;
				while (ip < end && *m == *ip) {
					m++, ip++;
				}
				m_len = ip - ii;
			}
			assert(m_len > M2_MAX_LEN);

			if (m_off <= M3_MAX_OFFSET) {
				m_off -= 1;
				if (m_len <= 33) {
					*op++ = byte(M3_MARKER | (m_len - 2));
				} else {
					m_len -= 33;
					*op++ = M3_MARKER | 0;
					goto m3_m4_len;
				}
			} else {
				m_off -= 0x4000;
				assert(m_off > 0); assert(m_off <= 0x7fff);
				if (m_len <= M4_MAX_LEN) {
					*op++ = byte(M4_MARKER | ((m_off & 0x4000) >> 11) | (m_len - 2));
				} else {
					m_len -= M4_MAX_LEN;
					*op++ = byte(M4_MARKER | ((m_off & 0x4000) >> 11));
m3_m4_len:
					while (m_len > 255) {
						m_len -= 255;
						*op++ = 0;
					}
					assert(m_len > 0);
					*op++ = byte(m_len);
				}
			}

m3_m4_offset:
			*op++ = byte((m_off & 63) << 2);
			*op++ = byte(m_off >> 6);
		}

		ii = ip;
		if (unlikely(ip >= ip_end)) {
			break;
		}
	}

	out_len = op - out;
	return in_end - ii;
}

void lzo1x_1_compress(const byte* in,  unsigned  in_len,
                            byte* out, unsigned& out_len)
{
	byte* op = out;

	unsigned t;
	if (unlikely(in_len <= M2_MAX_LEN + 5)) {
		t = in_len;
	} else {
		t = _lzo1x_1_do_compress(in, in_len, op, out_len);
		op += out_len;
	}

	if (t > 0) {
		const byte* ii = in + in_len - t;

		if (op == out && t <= 238) {
			*op++ = byte(17 + t);
		} else if (t <= 3) {
			op[-2] |= byte(t);
		} else if (t <= 18) {
			*op++ = byte(t - 3);
		} else {
			unsigned tt = t - 18;

			*op++ = 0;
			while (tt > 255) {
				tt -= 255;
				*op++ = 0;
			}
			assert(tt > 0);
			*op++ = byte(tt);
		}
		do { *op++ = *ii++; } while (--t > 0);
	}

	*op++ = M4_MARKER | 1;
	*op++ = 0;
	*op++ = 0;

	out_len = op - out;
}

// TODO: This function was copy-pasted from CPUCore.cc.
static ALWAYS_INLINE
unsigned read16LE(const byte* p)
{
	if (OPENMSX_BIGENDIAN || !OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		return p[0] + 256 * p[1];
	} else {
		return *reinterpret_cast<const word*>(p);
	}
}

/**
 * Copies "count" bytes from "src" to "dst".
 * If "dst" is inside the source memory region, the region [src..dst) is
 * repeated in the end result.
 * Both "src" and "dst" are incremented by "count".
 */
static ALWAYS_INLINE
void copyRepeat(byte*& dst, const byte*& src, unsigned count)
{
	for (/**/; count; count--) {
		*dst++ = *src++;
	}
}

void lzo1x_decompress(const byte* __restrict src, unsigned  src_len,
	                    byte* __restrict dst, unsigned& dst_len)
{
	unsigned t;

	byte* op = dst;
	const byte* ip = src;
	const byte* const src_end = src + src_len;

	dst_len = 0;

	if (*ip > 17) {
		t = *ip++ - 17;
		if (t < 4) {
			goto match_next;
		} else {
			copyRepeat(op, ip, t);
			goto first_literal_run;
		}
	}

	while (true) {
		t = *ip++;
		if (t < 16) {
			if (t == 0) {
				while (*ip == 0) {
					t += 255;
					ip++;
				}
				t += 15 + *ip++;
			}
			assert(t > 0);
			copyRepeat(op, ip, t + 3);

first_literal_run:
			t = *ip++;
			if (t < 16) {
				const byte* m_pos = op - (1 + M2_MAX_OFFSET);
				m_pos -= t >> 2;
				m_pos -= *ip++ << 2;
				copyRepeat(op, m_pos, 3);
				goto match_done;
			}
		}

		while (true) {
			const byte* m_pos;
			if (t >= 64) {
				m_pos = op - 1;
				m_pos -= (t >> 2) & 7;
				m_pos -= *ip++ << 3;
				t = (t >> 5) - 1;
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
					assert(t == 1);
					dst_len = op - dst;
					assert(ip == src_end); (void)src_end;
					return;
				}
				m_pos -= 0x4000;
			} else {
				m_pos = op - 1;
				m_pos -= t >> 2;
				m_pos -= *ip++ << 2;
				t = 0;
			}
			copyRepeat(op, m_pos, t + 2);

match_done:
			t = ip[-2] & 3;
			if (t == 0) {
				break;
			}

match_next:
			assert(t > 0); assert(t < 4);
			copyRepeat(op, ip, t);
			t = *ip++;
		}
	}
}

} // namespace openmsx
