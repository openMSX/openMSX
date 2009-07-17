// $Id$

#include "Math.hh"
#include "yuv2rgb.hh"
#include "likely.hh"

#include <cassert>

namespace openmsx {
namespace yuv2rgb {

static int coefs_gu[256];
static int coefs_gv[256];
static int coefs_bu[256];
static int coefs_rv[256];
static int coefs_y[256];

static const int PREC = 15;

static const int COEF_Y = (int)(1.164 * (1 << PREC) + 0.5);
static const int COEF_RV = (int)(1.596 * (1 << PREC) + 0.5);
static const int COEF_GU = (int)(0.391 * (1 << PREC) + 0.5);
static const int COEF_GV = (int)(0.813 * (1 << PREC) + 0.5);
static const int COEF_BU = (int)(2.018 * (1 << PREC) + 0.5);

static inline byte clamp(int x)
{
	STATIC_ASSERT((-1 >> 1) == -1); // right-shift must preserve sign
	return likely(byte(x) == x) ? x : ~(x >> 31);
//	return Math::clip<0, 255>(x);
}

void convert(byte __restrict *rawFrame, const yuv_buffer *buffer)
{
	static bool init = false;

	if (!init) {
		for (int i=0; i<256; i++) {
			coefs_gu[i] = -COEF_GU * (i - 128);
			coefs_gv[i] = -COEF_GV * (i - 128);
			coefs_bu[i] = COEF_BU * (i - 128);
			coefs_rv[i] = COEF_RV * (i - 128);
			coefs_y[i] = COEF_Y * (i - 16) + (PREC / 2);
		}

		init = true;
	}

	int x, y, Y;
	const byte *pY = buffer->y, *pCb = buffer->u, *pCr = buffer->v;
	const int y_stride = buffer->y_stride;
	const int width = buffer->y_width;

	assert(buffer->uv_width * 2 == buffer->y_width);
	assert(buffer->uv_height * 2 == buffer->y_height);

	for (y=0; y<buffer->y_height; y += 2) {
		pY = buffer->y + y * buffer->y_stride;
		pCb = buffer->u + y * buffer->uv_stride / 2;
		pCr = buffer->v + y * buffer->uv_stride / 2;
		
		for (x=0; x<width; x += 2) {
			int ruv, guv, buv;

			ruv = coefs_rv[*pCr];
			guv = coefs_gu[*pCb] + coefs_gv[*pCr];
			buv = coefs_bu[*pCb];

			int r, g, b, l;

			for (l=0; l<2; l++, pY++) {
				Y = coefs_y[*pY];
				r = (Y + ruv) >> PREC;
				g = (Y + guv) >> PREC;
				b = (Y + buv) >> PREC;

				rawFrame[0] = clamp(r);
				rawFrame[1] = clamp(g);
				rawFrame[2] = clamp(b);

				Y = coefs_y[pY[y_stride]];
				r = (Y + ruv) >> PREC;
				g = (Y + guv) >> PREC;
				b = (Y + buv) >> PREC;

				rawFrame[width*3+0] = clamp(r);
				rawFrame[width*3+1] = clamp(g);
				rawFrame[width*3+2] = clamp(b);
				rawFrame += 3;
			}

			pCr++;
			pCb++;
		}
		rawFrame += width*3;
	} 
}

} // namespace yuv2rgb
} // namespace openmsx
