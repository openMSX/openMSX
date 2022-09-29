#include "BitmapConverter.hh"
#include "endian.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include "components.hh"
#include <algorithm>
#include <cstdint>
#include <tuple>

namespace openmsx {

template<std::unsigned_integral Pixel>
BitmapConverter<Pixel>::BitmapConverter(
		std::span<const Pixel, 16 * 2> palette16_,
		std::span<const Pixel, 256>    palette256_,
		std::span<const Pixel, 32768>  palette32768_)
	: palette16(palette16_)
	, palette256(palette256_)
	, palette32768(palette32768_)
{
}

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::calcDPalette()
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

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::convertLine(std::span<Pixel> buf, std::span<const byte, 128> vramPtr)
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

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::convertLinePlanar(
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

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::renderGraphic4(
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
	if ((sizeof(Pixel) == 2) && ((uintptr_t(pixelPtr) & 1) == 1)) {
		// Its 16 bit destination but currently not aligned on a word boundary
		// First write one pixel to get aligned
		// Then write double pixels in a loop with 4 double pixels (is 8 single pixels) per iteration
		// Finally write the last pixel unaligned
		const auto* in  = reinterpret_cast<const unsigned*>(vramPtr0.data());
		unsigned data = in[0];
		if constexpr (Endian::BIG) {
			pixelPtr[0] = palette16[(data >> 28) & 0x0F];
			data <<=4;
		} else {
			pixelPtr[0] = palette16[(data >>  0) & 0x0F];
			data >>=4;
		}

		pixelPtr += 1; // Move to next pixel, which is on word boundary
		auto out = reinterpret_cast<DPixel*>(pixelPtr);
		for (auto i : xrange(256 / 8)) {
			// 8 pixels per iteration
			if constexpr (Endian::BIG) {
				out[4 * i + 0] = dPalette[(data >> 24) & 0xFF];
				out[4 * i + 1] = dPalette[(data >> 16) & 0xFF];
				out[4 * i + 2] = dPalette[(data >>  8) & 0xFF];
				if (i == (256-8) / 8) {
					// Last pixel in last iteration must be written individually
					pixelPtr[254] = palette16[(data >> 0) & 0x0F];
				} else {
					// Last double-pixel must be composed of
					// remaining 4 bits in (previous) data
					// and first 4 bits from (next) data
					unsigned prevData = data;
					data = in[i+1];
					out[4 * i + 3] = dPalette[(prevData & 0xF0) | ((data >> 28) & 0x0F)];
					data <<= 4;
				}
			} else {
				out[4 * i + 0] = dPalette[(data >>  0) & 0xFF];
				out[4 * i + 1] = dPalette[(data >>  8) & 0xFF];
				out[4 * i + 2] = dPalette[(data >> 16) & 0xFF];
				if (i != (256-8) / 8) {
					// Last pixel in last iteration must be written individually
					pixelPtr[254] = palette16[(data >> 24) & 0x0F];
				} else {
					// Last double-pixel must be composed of
					// remaining 4 bits in (previous) data
					// and first 4 bits from (next) data
					unsigned prevData = data;
					data = in[i+1];
					out[4 * i + 3] = dPalette[((prevData >> 24) & 0x0F) | ((data & 0x0F)<<4)];
					data >>=4;
				}
			}
		}
		return;
	}

	      auto* out = reinterpret_cast<DPixel*>(pixelPtr);
	const auto* in  = reinterpret_cast<const unsigned*>(vramPtr0.data());
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

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::renderGraphic5(
	std::span<Pixel, 512> buf,
	std::span<const byte, 128> vramPtr0)
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

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::renderGraphic6(
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
	      auto* out = reinterpret_cast<DPixel*>(pixelPtr);
	const auto* in0 = reinterpret_cast<const unsigned*>(vramPtr0.data());
	const auto* in1 = reinterpret_cast<const unsigned*>(vramPtr1.data());
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

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::renderGraphic7(
	std::span<Pixel, 256> buf,
	std::span<const byte, 128> vramPtr0,
	std::span<const byte, 128> vramPtr1)
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

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::renderYJK(
	std::span<Pixel, 256> buf,
	std::span<const byte, 128> vramPtr0,
	std::span<const byte, 128> vramPtr1)
{
	Pixel* __restrict pixelPtr = buf.data();
	for (auto i : xrange(64)) {
		unsigned p[4];
		p[0] = vramPtr0[2 * i + 0];
		p[1] = vramPtr1[2 * i + 0];
		p[2] = vramPtr0[2 * i + 1];
		p[3] = vramPtr1[2 * i + 1];

		int j = (p[2] & 7) + ((p[3] & 3) << 3) - ((p[3] & 4) << 3);
		int k = (p[0] & 7) + ((p[1] & 3) << 3) - ((p[1] & 4) << 3);

		for (auto n : xrange(4)) {
			int y = p[n] >> 3;
			auto [r, g, b] = yjk2rgb(y, j, k);
			int col = (r << 10) + (g << 5) + b;
			pixelPtr[4 * i + n] = palette32768[col];
		}
	}
}

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::renderYAE(
	std::span<Pixel, 256> buf,
	std::span<const byte, 128> vramPtr0,
	std::span<const byte, 128> vramPtr1)
{
	Pixel* __restrict pixelPtr = buf.data();
	for (auto i : xrange(64)) {
		unsigned p[4];
		p[0] = vramPtr0[2 * i + 0];
		p[1] = vramPtr1[2 * i + 0];
		p[2] = vramPtr0[2 * i + 1];
		p[3] = vramPtr1[2 * i + 1];

		int j = (p[2] & 7) + ((p[3] & 3) << 3) - ((p[3] & 4) << 3);
		int k = (p[0] & 7) + ((p[1] & 3) << 3) - ((p[1] & 4) << 3);

		for (auto n : xrange(4)) {
			Pixel pix;
			if (p[n] & 0x08) {
				// YAE
				pix = palette16[p[n] >> 4];
			} else {
				// YJK
				int y = p[n] >> 3;
				auto [r, g, b] = yjk2rgb(y, j, k);
				pix = palette32768[(r << 10) + (g << 5) + b];
			}
			pixelPtr[4 * i + n] = pix;
		}
	}
}

template<std::unsigned_integral Pixel>
void BitmapConverter<Pixel>::renderBogus(std::span<Pixel, 256> buf)
{
	// Verified on real V9958: all bogus modes behave like this, always
	// show palette color 15.
	// When this is in effect, the VRAM is not refreshed anymore, but that
	// is not emulated.
	ranges::fill(buf, palette16[15]);
}

// Force template instantiation.
#if HAVE_16BPP
template class BitmapConverter<uint16_t>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class BitmapConverter<uint32_t>;
#endif

} // namespace openmsx
