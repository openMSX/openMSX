#include "OutputSurface.hh"
#include "unreachable.hh"
#include "build-info.hh"

namespace openmsx {

void OutputSurface::lock()
{
	if (isLocked()) return;
	locked = true;
	if (surface && SDL_MUSTLOCK(surface)) {
		// Note: we ignore the return value from SDL_LockSurface()
		SDL_LockSurface(surface);
	}
}

void OutputSurface::unlock()
{
	if (!isLocked()) return;
	locked = false;
	if (surface && SDL_MUSTLOCK(surface)) {
		SDL_UnlockSurface(surface);
	}
}

void OutputSurface::calculateViewPort(gl::ivec2 physSize_)
{
	m_physSize = physSize_;
	gl::vec2 physSize(physSize_);

	gl::vec2 logSize = gl::vec2(getLogicalSize());
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

void OutputSurface::setBufferPtr(char* data_, unsigned pitch_)
{
	data = data_;
	pitch = pitch_;
}

void OutputSurface::flushFrameBuffer()
{
}

} // namespace openmsx
