#include "BitmapConverter.hh"

#include "endian.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <algorithm>
#include <bit>
#include <tuple>

namespace openmsx {

BitmapConverter::BitmapConverter(
		std::span<const Pixel, 16 * 2> palette16_,
		std::span<const Pixel, 256>    palette256_,
		std::span<const Pixel, 32768>  palette32768_)
	: palette16(palette16_)
	, palette256(palette256_)
	, palette32768(palette32768_)
{
}

void BitmapConverter::calcDPalette()
{
	dPaletteValid = true;
	unsigned bits = sizeof(Pixel) * 8;
	for (auto i : xrange(16)) {
		DPixel p0 = palette16[i];
		for (auto j : xrange(16)) {
			DPixel p1 = palette16[j];
			DPixel dp = Endian::BIG ? (p0 << bits) | p1
			                        : (p1 << bits) | p0;
			dPalette[16 * i + j] = dp;
		}
	}
}

void BitmapConverter::convertLine(std::span<Pixel> buf, std::span<const byte, 128> vramPtr)
{
	switch (mode.getByte()) {
	case DisplayMode::GRAPHIC4: // screen 5
	case DisplayMode::GRAPHIC4 | DisplayMode::YAE:
		renderGraphic4(subspan<256>(buf), vramPtr);
		break;
	case DisplayMode::GRAPHIC5: // screen 6
	case DisplayMode::GRAPHIC5 | DisplayMode::YAE:
		renderGraphic5(subspan<512>(buf), vramPtr);
		break;
	// These are handled in convertLinePlanar().
	case DisplayMode::GRAPHIC6:
	case DisplayMode::GRAPHIC7:
	case DisplayMode::GRAPHIC6 |                    DisplayMode::YAE:
	case DisplayMode::GRAPHIC7 |                    DisplayMode::YAE:
	case DisplayMode::GRAPHIC6 | DisplayMode::YJK:
	case DisplayMode::GRAPHIC7 | DisplayMode::YJK:
	case DisplayMode::GRAPHIC6 | DisplayMode::YJK | DisplayMode::YAE:
	case DisplayMode::GRAPHIC7 | DisplayMode::YJK | DisplayMode::YAE:
		UNREACHABLE;
	// TODO: Support YJK on modes other than Graphic 6/7.
	case DisplayMode::GRAPHIC4 | DisplayMode::YJK:
	case DisplayMode::GRAPHIC5 | DisplayMode::YJK:
	case DisplayMode::GRAPHIC4 | DisplayMode::YJK | DisplayMode::YAE:
	case DisplayMode::GRAPHIC5 | DisplayMode::YJK | DisplayMode::YAE:
	default:
		renderBogus(subspan<256>(buf));
		break;
	}
}

void BitmapConverter::convertLinePlanar(
	std::span<Pixel> buf, std::span<const byte, 128> vramPtr0, std::span<const byte, 128> vramPtr1)
{
	switch (mode.getByte()) {
	case DisplayMode::GRAPHIC6: // screen 7
	case DisplayMode::GRAPHIC6 | DisplayMode::YAE:
		renderGraphic6(subspan<512>(buf), vramPtr0, vramPtr1);
		break;
	case DisplayMode::GRAPHIC7: // screen 8
	case DisplayMode::GRAPHIC7 | DisplayMode::YAE:
		renderGraphic7(subspan<256>(buf), vramPtr0, vramPtr1);
		break;
	case DisplayMode::GRAPHIC6 | DisplayMode::YJK: // screen 12
	case DisplayMode::GRAPHIC7 | DisplayMode::YJK:
		renderYJK(subspan<256>(buf), vramPtr0, vramPtr1);
		break;
	case DisplayMode::GRAPHIC6 | DisplayMode::YJK | DisplayMode::YAE: // screen 11
	case DisplayMode::GRAPHIC7 | DisplayMode::YJK | DisplayMode::YAE:
		renderYAE(subspan<256>(buf), vramPtr0, vramPtr1);
		break;
	// These are handled in convertLine().
	case DisplayMode::GRAPHIC4:
	case DisplayMode::GRAPHIC5:
	case DisplayMode::GRAPHIC4 |                    DisplayMode::YAE:
	case DisplayMode::GRAPHIC5 |                    DisplayMode::YAE:
	case DisplayMode::GRAPHIC4 | DisplayMode::YJK:
	case DisplayMode::GRAPHIC5 | DisplayMode::YJK:
	case DisplayMode::GRAPHIC4 | DisplayMode::YJK | DisplayMode::YAE:
	case DisplayMode::GRAPHIC5 | DisplayMode::YJK | DisplayMode::YAE:
		UNREACHABLE;
	default:
		renderBogus(subspan<256>(buf));
		break;
	}
}

void BitmapConverter::renderGraphic4(
	std::span<Pixel, 256> buf,
	std::span<const byte, 128> vramPtr0)
{
	/*for (unsigned i = 0; i < 128; i += 2) {
		unsigned data0 = vramPtr0[i + 0];
		unsigned data1 = vramPtr0[i + 1];
		buf[2 * i + 0] = palette16[data0 >> 4];
		buf[2 * i + 1] = palette16[data0 & 15];
		buf[2 * i + 2] = palette16[data1 >> 4];
		buf[2 * i + 3] = palette16[data1 & 15];
	}*/

	if (!dPaletteValid) [[unlikely]] {
		calcDPalette();
	}

	Pixel* __restrict pixelPtr = buf.data();
	      auto* out = std::bit_cast<DPixel*>(pixelPtr);
	const auto* in  = std::bit_cast<const unsigned*>(vramPtr0.data());
	for (auto i : xrange(256 / 8)) {
		// 8 pixels per iteration
		unsigned data = in[i];
		if constexpr (Endian::BIG) {
			out[4 * i + 0] = dPalette[(data >> 24) & 0xFF];
			out[4 * i + 1] = dPalette[(data >> 16) & 0xFF];
			out[4 * i + 2] = dPalette[(data >>  8) & 0xFF];
			out[4 * i + 3] = dPalette[(data >>  0) & 0xFF];
		} else {
			out[4 * i + 0] = dPalette[(data >>  0) & 0xFF];
			out[4 * i + 1] = dPalette[(data >>  8) & 0xFF];
			out[4 * i + 2] = dPalette[(data >> 16) & 0xFF];
			out[4 * i + 3] = dPalette[(data >> 24) & 0xFF];
		}
	}
}

void BitmapConverter::renderGraphic5(
	std::span<Pixel, 512> buf,
	std::span<const byte, 128> vramPtr0) const
{
	Pixel* __restrict pixelPtr = buf.data();
	for (auto i : xrange(128)) {
		unsigned data = vramPtr0[i];
		pixelPtr[4 * i + 0] = palette16[ 0 +  (data >> 6)     ];
		pixelPtr[4 * i + 1] = palette16[16 + ((data >> 4) & 3)];
		pixelPtr[4 * i + 2] = palette16[ 0 + ((data >> 2) & 3)];
		pixelPtr[4 * i + 3] = palette16[16 + ((data >> 0) & 3)];
	}
}

void BitmapConverter::renderGraphic6(
	std::span<Pixel, 512> buf,
	std::span<const byte, 128> vramPtr0,
	std::span<const byte, 128> vramPtr1)
{
	Pixel* __restrict pixelPtr = buf.data();
	/*for (auto i : xrange(128)) {
		unsigned data0 = vramPtr0[i];
		unsigned data1 = vramPtr1[i];
		pixelPtr[4 * i + 0] = palette16[data0 >> 4];
		pixelPtr[4 * i + 1] = palette16[data0 & 15];
		pixelPtr[4 * i + 2] = palette16[data1 >> 4];
		pixelPtr[4 * i + 3] = palette16[data1 & 15];
	}*/
	if (!dPaletteValid) [[unlikely]] {
		calcDPalette();
	}
	      auto* out = std::bit_cast<DPixel*>(pixelPtr);
	const auto* in0 = std::bit_cast<const unsigned*>(vramPtr0.data());
	const auto* in1 = std::bit_cast<const unsigned*>(vramPtr1.data());
	for (auto i : xrange(512 / 16)) {
		// 16 pixels per iteration
		unsigned data0 = in0[i];
		unsigned data1 = in1[i];
		if constexpr (Endian::BIG) {
			out[8 * i + 0] = dPalette[(data0 >> 24) & 0xFF];
			out[8 * i + 1] = dPalette[(data1 >> 24) & 0xFF];
			out[8 * i + 2] = dPalette[(data0 >> 16) & 0xFF];
			out[8 * i + 3] = dPalette[(data1 >> 16) & 0xFF];
			out[8 * i + 4] = dPalette[(data0 >>  8) & 0xFF];
			out[8 * i + 5] = dPalette[(data1 >>  8) & 0xFF];
			out[8 * i + 6] = dPalette[(data0 >>  0) & 0xFF];
			out[8 * i + 7] = dPalette[(data1 >>  0) & 0xFF];
		} else {
			out[8 * i + 0] = dPalette[(data0 >>  0) & 0xFF];
			out[8 * i + 1] = dPalette[(data1 >>  0) & 0xFF];
			out[8 * i + 2] = dPalette[(data0 >>  8) & 0xFF];
			out[8 * i + 3] = dPalette[(data1 >>  8) & 0xFF];
			out[8 * i + 4] = dPalette[(data0 >> 16) & 0xFF];
			out[8 * i + 5] = dPalette[(data1 >> 16) & 0xFF];
			out[8 * i + 6] = dPalette[(data0 >> 24) & 0xFF];
			out[8 * i + 7] = dPalette[(data1 >> 24) & 0xFF];
		}
	}
}

void BitmapConverter::renderGraphic7(
	std::span<Pixel, 256> buf,
	std::span<const byte, 128> vramPtr0,
	std::span<const byte, 128> vramPtr1) const
{
	Pixel* __restrict pixelPtr = buf.data();
	for (auto i : xrange(128)) {
		pixelPtr[2 * i + 0] = palette256[vramPtr0[i]];
		pixelPtr[2 * i + 1] = palette256[vramPtr1[i]];
	}
}

static constexpr std::tuple<int, int, int> yjk2rgb(int y, int j, int k)
{
	// Note the formula for 'blue' differs from the 'traditional' formula
	// (e.g. as specified in the V9958 datasheet) in the rounding behavior.
	// Confirmed on real turbor machine. For details see:
	//    https://github.com/openMSX/openMSX/issues/1394
	//    https://twitter.com/mdpc___/status/1480432007180341251?s=20
	int r = std::clamp(y + j,                       0, 31);
	int g = std::clamp(y + k,                       0, 31);
	int b = std::clamp((5 * y - 2 * j - k + 2) / 4, 0, 31);
	return {r, g, b};
}

void BitmapConverter::renderYJK(
	std::span<Pixel, 256> buf,
	std::span<const byte, 128> vramPtr0,
	std::span<const byte, 128> vramPtr1) const
{
	Pixel* __restrict pixelPtr = buf.data();
	for (auto i : xrange(64)) {
		std::array<unsigned, 4> p = {
			vramPtr0[2 * i + 0],
			vramPtr1[2 * i + 0],
			vramPtr0[2 * i + 1],
			vramPtr1[2 * i + 1],
		};
		int j = narrow<int>((p[2] & 7) + ((p[3] & 3) << 3)) - narrow<int>((p[3] & 4) << 3);
		int k = narrow<int>((p[0] & 7) + ((p[1] & 3) << 3)) - narrow<int>((p[1] & 4) << 3);

		for (auto n : xrange(4)) {
			int y = narrow<int>(p[n] >> 3);
			auto [r, g, b] = yjk2rgb(y, j, k);
			int col = (r << 10) + (g << 5) + b;
			pixelPtr[4 * i + n] = palette32768[col];
		}
	}
}

void BitmapConverter::renderYAE(
	std::span<Pixel, 256> buf,
	std::span<const byte, 128> vramPtr0,
	std::span<const byte, 128> vramPtr1) const
{
	Pixel* __restrict pixelPtr = buf.data();
	for (auto i : xrange(64)) {
		std::array<unsigned, 4> p = {
			vramPtr0[2 * i + 0],
			vramPtr1[2 * i + 0],
			vramPtr0[2 * i + 1],
			vramPtr1[2 * i + 1],
		};
		int j = narrow<int>((p[2] & 7) + ((p[3] & 3) << 3)) - narrow<int>((p[3] & 4) << 3);
		int k = narrow<int>((p[0] & 7) + ((p[1] & 3) << 3)) - narrow<int>((p[1] & 4) << 3);

		for (auto n : xrange(4)) {
			Pixel pix;
			if (p[n] & 0x08) {
				// YAE
				pix = palette16[p[n] >> 4];
			} else {
				// YJK
				int y = narrow<int>(p[n] >> 3);
				auto [r, g, b] = yjk2rgb(y, j, k);
				pix = palette32768[(r << 10) + (g << 5) + b];
			}
			pixelPtr[4 * i + n] = pix;
		}
	}
}

void BitmapConverter::renderBogus(std::span<Pixel, 256> buf) const
{
	// Verified on real V9958: all bogus modes behave like this, always
	// show palette color 15.
	// When this is in effect, the VRAM is not refreshed anymore, but that
	// is not emulated.
	ranges::fill(buf, palette16[15]);
}

} // namespace openmsx
