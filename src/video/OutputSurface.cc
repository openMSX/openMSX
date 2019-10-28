#include "OutputSurface.hh"
#include "unreachable.hh"
#include "build-info.hh"

namespace openmsx {

void OutputSurface::calculateViewPort(gl::ivec2 logSize_, gl::ivec2 physSize_)
{
	m_logicalSize = logSize_;
	m_physSize = physSize_;

	gl::vec2 logSize(logSize_); // convert int->float
	gl::vec2 physSize(physSize_);

	float scale = min_component(physSize / logSize);
	m_viewScale = gl::vec2(scale); // for now always same X and Y scale

	gl::vec2 viewSize = logSize * scale;
	m_viewSize = round(viewSize);

	gl::vec2 viewOffset = (physSize - viewSize) / 2.0f;
	m_viewOffset = round(viewOffset);
}

void OutputSurface::setSDLFormat(const SDL_PixelFormat& format_)
{
	format = format_;
#if HAVE_32BPP
	// SDL sets an alpha channel only for GL modes. We want an alpha channel
	// for all 32bpp output surfaces, so we add one ourselves if necessary.
	if (format.BytesPerPixel == 4 && format.Amask == 0) {
		unsigned rgbMask = format.Rmask | format.Gmask | format.Bmask;
		if ((rgbMask & 0x000000FF) == 0) {
			format.Amask  = 0x000000FF;
			format.Ashift = 0;
			format.Aloss  = 0;
		} else if ((rgbMask & 0xFF000000) == 0) {
			format.Amask  = 0xFF000000;
			format.Ashift = 24;
			format.Aloss  = 0;
		} else {
			UNREACHABLE;
		}
	}
#endif
}

} // namespace openmsx
