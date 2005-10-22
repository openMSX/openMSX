// $Id$

#ifndef HQCOMMON_HH
#define HQCOMMON_HH

namespace openmsx {

template <class Pixel>
static inline unsigned readPixel(const Pixel* pIn)
{
	// TODO: Use surface info instead.
	Pixel p = *pIn;
	if (sizeof(Pixel) == 2) {
		return ((p & 0xF800) << 8) |
		       ((p & 0x07C0) << 5) | // drop lowest green bit
		       ((p & 0x001F) << 3);
	} else {
		return p & 0xF8F8F8;
	}
}

template <class Pixel>
static inline void pset(Pixel* pOut, unsigned p)
{
	// TODO: Use surface info instead.
	if (sizeof(Pixel) == 2) {
		*pOut = ((p & 0xF80000) >> 8) |
			((p & 0x00FC00) >> 5) |
			((p & 0x0000F8) >> 3);
	} else {
		*pOut = (p & 0xF8F8F8) | ((p & 0xE0E0E0) >> 5);
	}
}

template <int w1, int w2>
static inline unsigned interpolate(unsigned c1, unsigned c2)
{
	enum { wsum = w1 + w2 };
	return (c1 * w1 + c2 * w2) / wsum;
}

template <int w1, int w2, int w3>
static inline unsigned interpolate(unsigned c1, unsigned c2, unsigned c3)
{
	enum { wsum = w1 + w2 + w3 };
	if (wsum <= 8) {
		// Because the lower 3 bits of each colour component (R,G,B) are
		// zeroed out, we can operate on a single integer as if it is
		// a vector.
		return (c1 * w1 + c2 * w2 + c3 * w3) / wsum;
	} else {
		return ((((c1 & 0x00FF00) * w1 +
		          (c2 & 0x00FF00) * w2 +
		          (c3 & 0x00FF00) * w3) & (0x00FF00 * wsum)) |
		        (((c1 & 0xFF00FF) * w1 +
		          (c2 & 0xFF00FF) * w2 +
		          (c3 & 0xFF00FF) * w3) & (0xFF00FF * wsum))) / wsum;
	}
}

static inline bool edge(unsigned c1, unsigned c2)
{
	if (c1 == c2) return false;

	unsigned r1 = c1 >> 16;
	unsigned g1 = (c1 >> 8) & 0xFF;
	unsigned b1 = c1 & 0xFF;
	unsigned y1 = r1 + g1 + b1;

	unsigned r2 = c2 >> 16;
	unsigned g2 = (c2 >> 8) & 0xFF;
	unsigned b2 = c2 & 0xFF;
	unsigned y2 = r2 + g2 + b2;

	int dy = y1 - y2;
	if (dy < -0xC0 || dy > 0xC0) return true;

	int du = r1 - r2 + b2 - b1;
	if (du < -0x1C || du > 0x1C) return true;

	int dv = 3 * (g1 - g2) - dy;
	if (dv < -0x30 || dv > 0x30) return true;

	return false;
}

} // namespace openmsx

#endif
