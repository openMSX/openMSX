// $Id$

#include "OutputSurface.hh"
#include "unreachable.hh"
#include "build-info.hh"

namespace openmsx {

OutputSurface::OutputSurface()
	: displaySurface(0), workSurface(0), xOffset(0), yOffset(0), locked(false)
{
}

OutputSurface::~OutputSurface()
{
}

void OutputSurface::lock()
{
	if (isLocked()) return;
	locked = true;
	if (workSurface && SDL_MUSTLOCK(workSurface)) {
		// Note: we ignore the return value from SDL_LockSurface()
		SDL_LockSurface(workSurface);
	}
}

void OutputSurface::unlock()
{
	if (!isLocked()) return;
	locked = false;
	if (workSurface && SDL_MUSTLOCK(workSurface)) {
		SDL_UnlockSurface(workSurface);
	}
}

void OutputSurface::setPosition(int x, int y)
{
	xOffset = x;
	yOffset = y;
}

unsigned OutputSurface::mapRGB(double dr, double dg, double db)
{
	int r = int(dr * 255.0);
	int g = int(dg * 255.0);
	int b = int(db * 255.0);
	return SDL_MapRGB(&format, r, g, b); // alpha is fully opaque
}

void OutputSurface::setSDLDisplaySurface(SDL_Surface* surface)
{
	displaySurface = surface;
}

void OutputSurface::setSDLWorkSurface(SDL_Surface* surface)
{
	workSurface = surface;
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
