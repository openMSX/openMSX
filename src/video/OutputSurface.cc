// $Id$

#include "OutputSurface.hh"

namespace openmsx {

OutputSurface::OutputSurface()
	: displaySurface(0), workSurface(0), keyColour(0), locked(false)
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

unsigned OutputSurface::mapRGB(double dr, double dg, double db)
{
	int r = int(dr * 255.0);
	int g = int(dg * 255.0);
	int b = int(db * 255.0);
	return SDL_MapRGB(&format, r, g, b);
}

bool OutputSurface::canKeyColourClash()
{
	return format.BitsPerPixel == 16;
}

void OutputSurface::generateNewKeyColour()
{
	assert(canKeyColourClash());

	keyColour = (keyColour + 1) & 0xffff;
}

unsigned OutputSurface::getKeyColour()
{
	switch (format.BitsPerPixel) {
	case 32:
	case 24:
		keyColour = 0xffffffff;
		break;
	case 15:
		keyColour = 0xffff;
	case 16:
		break;
	default:
		assert(false);
	}

	return keyColour;
}

void OutputSurface::setSDLDisplaySurface(SDL_Surface* surface)
{
	displaySurface = surface;
}

void OutputSurface::setSDLWorkSurface(SDL_Surface* surface)
{
	workSurface = surface;
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
