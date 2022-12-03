#include "SDLSnow.hh"
#include "SDLOutputSurface.hh"
#include "Display.hh"
#include "build-info.hh"
#include "checked_cast.hh"
#include "enumerate.hh"
#include "random.hh"
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
SDLSnow<Pixel>::SDLSnow(OutputSurface& output, Display& display_)
	: Layer(COVER_FULL, Z_BACKGROUND)
	, display(display_)
{
	// Precalc gray values for noise
	for (auto [i, g] : enumerate(gray)) {
		g = Pixel(output.mapRGB255(gl::ivec3(int(i))));
	}
}

template<std::unsigned_integral Pixel>
void SDLSnow<Pixel>::paint(OutputSurface& output_)
{
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::uniform_int_distribution<int> distribution(0, 255);

	auto& output = checked_cast<SDLOutputSurface&>(output_);
	{
		auto pixelAccess = output.getDirectPixelAccess();
		auto [width, height] = output.getLogicalSize();
		for (int y = 0; y < height; y += 2) {
			auto p0 = pixelAccess.getLine<Pixel>(y + 0).subspan(0, width);
			for (int x = 0; x < width; x += 2) {
				p0[x + 0] = p0[x + 1] = gray[distribution(generator)];
			}
			auto p1 = pixelAccess.getLine<Pixel>(y + 1).subspan(0, width);
			ranges::copy(p0, p1);
		}
	}
	output.flushFrameBuffer();

	display.repaintDelayed(100 * 1000); // 10fps
}

// Force template instantiation.
#if HAVE_16BPP
template class SDLSnow<uint16_t>;
#endif
#if HAVE_32BPP
template class SDLSnow<uint32_t>;
#endif

} // namespace openmsx
