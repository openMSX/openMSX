// $Id$

#include "yuv2rgb.hh"
#include "Math.hh"
#include <cassert>

namespace openmsx {
namespace yuv2rgb {

static int coefs_gu[256];
static int coefs_gv[256];
static int coefs_bu[256];
static int coefs_rv[256];
static int coefs_y [256];

static const int PREC = 15;
static const int COEF_Y  = int(1.164 * (1 << PREC) + 0.5);
static const int COEF_RV = int(1.596 * (1 << PREC) + 0.5);
static const int COEF_GU = int(0.391 * (1 << PREC) + 0.5);
static const int COEF_GV = int(0.813 * (1 << PREC) + 0.5);
static const int COEF_BU = int(2.018 * (1 << PREC) + 0.5);

void convert(byte* __restrict rawFrame, const yuv_buffer* buffer)
{
	static bool init = false;
	if (!init) {
		init = true;
		for (int i = 0; i < 256; ++i) {
			coefs_gu[i] = -COEF_GU * (i - 128);
			coefs_gv[i] = -COEF_GV * (i - 128);
			coefs_bu[i] =  COEF_BU * (i - 128);
			coefs_rv[i] =  COEF_RV * (i - 128);
			coefs_y[i]  =  COEF_Y  * (i -  16) + (PREC / 2);
		}
	}

	assert(buffer->uv_width  * 2 == buffer->y_width);
	assert(buffer->uv_height * 2 == buffer->y_height);

	const int width      = buffer->y_width;
	const int y_stride   = buffer->y_stride;
	const int uv_stride2 = buffer->uv_stride / 2;
	const int raw_stride = 3 * width;

	for (int y = 0; y < buffer->y_height; y += 2) {
		const byte* pY  = buffer->y + y * y_stride;
		const byte* pCb = buffer->u + y * uv_stride2;
		const byte* pCr = buffer->v + y * uv_stride2;
		byte* out0      = rawFrame  + y * raw_stride;
		byte* out1      = out0      +     raw_stride;

		for (int x = 0; x < width;
		     x += 2, pY += 2, ++pCr, ++pCb, out0 += 6, out1 += 6) {
			int ruv = coefs_rv[*pCr];
			int guv = coefs_gu[*pCb] + coefs_gv[*pCr];
			int buv = coefs_bu[*pCb];

			int Y00 = coefs_y[pY[0]];
			out0[0] = Math::clipIntToByte((Y00 + ruv) >> PREC);
			out0[1] = Math::clipIntToByte((Y00 + guv) >> PREC);
			out0[2] = Math::clipIntToByte((Y00 + buv) >> PREC);

			int Y01 = coefs_y[pY[1]];
			out0[3] = Math::clipIntToByte((Y01 + ruv) >> PREC);
			out0[4] = Math::clipIntToByte((Y01 + guv) >> PREC);
			out0[5] = Math::clipIntToByte((Y01 + buv) >> PREC);

			int Y10 = coefs_y[pY[y_stride + 0]];
			out1[0] = Math::clipIntToByte((Y10 + ruv) >> PREC);
			out1[1] = Math::clipIntToByte((Y10 + guv) >> PREC);
			out1[2] = Math::clipIntToByte((Y10 + buv) >> PREC);

			int Y11 = coefs_y[pY[y_stride + 1]];
			out1[3] = Math::clipIntToByte((Y11 + ruv) >> PREC);
			out1[4] = Math::clipIntToByte((Y11 + guv) >> PREC);
			out1[5] = Math::clipIntToByte((Y11 + buv) >> PREC);
		}
	}
}

} // namespace yuv2rgb
} // namespace openmsx
