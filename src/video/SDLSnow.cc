#include "SDLSnow.hh"
#include "SDLOutputSurface.hh"
#include "Display.hh"
#include "build-info.hh"
#include "checked_cast.hh"
#include "enumerate.hh"
#include "random.hh"
#include <cstring>
#include <cstdint>

namespace openmsx {

template<typename Pixel>
SDLSnow<Pixel>::SDLSnow(OutputSurface& output, Display& display_)
	: Layer(COVER_FULL, Z_BACKGROUND)
	, display(display_)
{
	// Precalc gray values for noise
	for (auto [i, g] : enumerate(gray)) {
		g = output.mapRGB255(gl::ivec3(int(i)));
	}
}

template<typename Pixel>
void SDLSnow<Pixel>::paint(OutputSurface& output_)
{
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::uniform_int_distribution<int> distribution(0, 255);

	auto& output = checked_cast<SDLOutputSurface&>(output_);
	{
		auto pixelAccess = output.getDirectPixelAccess();
		auto [width, height] = output.getLogicalSize();
		for (int y = 0; y < height; y += 2) {
			auto* p0 = pixelAccess.getLinePtr<Pixel>(y + 0);
			auto* p1 = pixelAccess.getLinePtr<Pixel>(y + 1);
			for (int x = 0; x < width; x += 2) {
				p0[x + 0] = p0[x + 1] = gray[distribution(generator)];
			}
			memcpy(p1, p0, width * sizeof(Pixel));
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
