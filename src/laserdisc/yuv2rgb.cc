// $Id$

#include "yuv2rgb.hh"
#include "RawFrame.hh"
#include "Math.hh"
#include <SDL.h>
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


static void initTables()
{
	static bool init = false;
	if (init) return;
	init = true;

	for (int i = 0; i < 256; ++i) {
		coefs_gu[i] = -COEF_GU * (i - 128);
		coefs_gv[i] = -COEF_GV * (i - 128);
		coefs_bu[i] =  COEF_BU * (i - 128);
		coefs_rv[i] =  COEF_RV * (i - 128);
		coefs_y[i]  =  COEF_Y  * (i -  16) + (PREC / 2);
	}
}

template<typename Pixel>
static inline Pixel calc(const SDL_PixelFormat& format,
                         int y, int ruv, int guv, int buv)
{
	byte r = Math::clipIntToByte((y + ruv) >> PREC);
	byte g = Math::clipIntToByte((y + guv) >> PREC);
	byte b = Math::clipIntToByte((y + buv) >> PREC);
	if (sizeof(Pixel) == 4) {
		return (r << 16) | (g << 8) | (b << 0);
	} else {
		return static_cast<Pixel>(SDL_MapRGB(&format, r, g, b));
	}
}

template<typename Pixel>
static void convertHelper(const yuv_buffer& buffer, RawFrame& output,
                          const SDL_PixelFormat& format)
{
	assert(buffer.uv_width  * 2 == buffer.y_width);
	assert(buffer.uv_height * 2 == buffer.y_height);

	const int width      = buffer.y_width;
	const int y_stride   = buffer.y_stride;
	const int uv_stride2 = buffer.uv_stride / 2;

	for (int y = 0; y < buffer.y_height; y += 2) {
		const byte* pY  = buffer.y + y * y_stride;
		const byte* pCb = buffer.u + y * uv_stride2;
		const byte* pCr = buffer.v + y * uv_stride2;
		Pixel* out0 = output.getLinePtrDirect<Pixel>(y + 0);
		Pixel* out1 = output.getLinePtrDirect<Pixel>(y + 1);

		for (int x = 0; x < width;
		     x += 2, pY += 2, ++pCr, ++pCb, out0 += 2, out1 += 2) {
			int ruv = coefs_rv[*pCr];
			int guv = coefs_gu[*pCb] + coefs_gv[*pCr];
			int buv = coefs_bu[*pCb];

			int Y00 = coefs_y[pY[0]];
			out0[0] = calc<Pixel>(format, Y00, ruv, guv, buv);

			int Y01 = coefs_y[pY[1]];
			out0[1] = calc<Pixel>(format, Y01, ruv, guv, buv);

			int Y10 = coefs_y[pY[y_stride + 0]];
			out1[0] = calc<Pixel>(format, Y10, ruv, guv, buv);

			int Y11 = coefs_y[pY[y_stride + 1]];
			out1[1] = calc<Pixel>(format, Y11, ruv, guv, buv);
		}

		output.setLineWidth(y + 0, width);
		output.setLineWidth(y + 1, width);
	}
}

void convert(const yuv_buffer& input, RawFrame& output)
{
	initTables();

	const SDL_PixelFormat& format = output.getSDLPixelFormat();
	if (format.BytesPerPixel == 4) {
		convertHelper<unsigned>(input, output, format);
	} else {
		assert(format.BytesPerPixel == 2);
		convertHelper<short>(input, output, format);
	}
}


} // namespace yuv2rgb
} // namespace openmsx
