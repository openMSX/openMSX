#include "yuv2rgb.hh"

#include "RawFrame.hh"

#include "Math.hh"
#include "xrange.hh"

#include <array>
#include <cassert>
#include <cstdint>

namespace openmsx::yuv2rgb {

using Pixel = uint32_t;

static constexpr int PREC = 15;
static constexpr int COEF_Y  = int(1.164 * (1 << PREC) + 0.5); // prefer to use lrint() to round
static constexpr int COEF_RV = int(1.596 * (1 << PREC) + 0.5); // but that's not (yet) constexpr
static constexpr int COEF_GU = int(0.391 * (1 << PREC) + 0.5); // in current versions of c++
static constexpr int COEF_GV = int(0.813 * (1 << PREC) + 0.5);
static constexpr int COEF_BU = int(2.018 * (1 << PREC) + 0.5);

struct Coefs {
	std::array<int, 256> gu;
	std::array<int, 256> gv;
	std::array<int, 256> bu;
	std::array<int, 256> rv;
	std::array<int, 256> y;
};

[[nodiscard]] static constexpr Coefs getCoefs()
{
	Coefs coefs = {};
	for (auto i : xrange(256)) {
		coefs.gu[i] = -COEF_GU * (i - 128);
		coefs.gv[i] = -COEF_GV * (i - 128);
		coefs.bu[i] =  COEF_BU * (i - 128);
		coefs.rv[i] =  COEF_RV * (i - 128);
		coefs.y[i]  =  COEF_Y  * (i -  16) + (PREC / 2);
	}
	return coefs;
}

[[nodiscard]] static inline Pixel calc(
	int y, int ruv, int guv, int buv)
{
	uint8_t r = Math::clipIntToByte((y + ruv) >> PREC);
	uint8_t g = Math::clipIntToByte((y + guv) >> PREC);
	uint8_t b = Math::clipIntToByte((y + buv) >> PREC);
	return (r << 0) | (g << 8) | (b << 16);
}

static void convertHelper(const th_ycbcr_buffer& buffer, RawFrame& output)
{
	assert(buffer[1].width  * 2 == buffer[0].width);
	assert(buffer[1].height * 2 == buffer[0].height);

	static constexpr Coefs coefs = getCoefs();

	const int    width      = buffer[0].width;
	const size_t y_stride   = buffer[0].stride;
	const size_t uv_stride2 = buffer[1].stride / 2;

	for (int y = 0; y < buffer[0].height; y += 2) {
		const uint8_t* pY  = buffer[0].data + y * y_stride;
		const uint8_t* pCb = buffer[1].data + y * uv_stride2;
		const uint8_t* pCr = buffer[2].data + y * uv_stride2;
		auto out0 = output.getLineDirect(y + 0);
		auto out1 = output.getLineDirect(y + 1);

		for (int x = 0; x < width; x += 2, pY += 2, ++pCr, ++pCb) {
			int ruv = coefs.rv[*pCr];
			int guv = coefs.gu[*pCb] + coefs.gv[*pCr];
			int buv = coefs.bu[*pCb];

			int Y00 = coefs.y[pY[0]];
			out0[x + 0] = calc(Y00, ruv, guv, buv);

			int Y01 = coefs.y[pY[1]];
			out0[x + 1] = calc(Y01, ruv, guv, buv);

			int Y10 = coefs.y[pY[y_stride + 0]];
			out1[x + 0] = calc(Y10, ruv, guv, buv);

			int Y11 = coefs.y[pY[y_stride + 1]];
			out1[x + 1] = calc(Y11, ruv, guv, buv);
		}

		output.setLineWidth(y + 0, width);
		output.setLineWidth(y + 1, width);
	}
}

void convert(const th_ycbcr_buffer& input, RawFrame& output)
{
	convertHelper(input, output);
}

} // namespace openmsx::yuv2rgb
