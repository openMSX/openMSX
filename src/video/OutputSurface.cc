// $Id$

#include "OutputSurface.hh"

namespace openmsx {

OutputSurface::OutputSurface()
	: surface(0), locked(false)
{
}

OutputSurface::~OutputSurface()
{
}

void OutputSurface::lock()
{
	if (isLocked()) return;
	locked = true;
	if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
	// Note: we ignore the return value from SDL_LockSurface()
}

void OutputSurface::unlock()
{
	if (!isLocked()) return;
	locked = false;
	if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
}

unsigned OutputSurface::mapRGB(double dr, double dg, double db)
{
	int r = int(dr * 255.0);
	int g = int(dg * 255.0);
	int b = int(db * 255.0);
	return SDL_MapRGB(&format, r, g, b);
}

void OutputSurface::setSDLSurface(SDL_Surface* surface_)
{
	surface = surface_;
}

void OutputSurface::setBufferPtr(char* data_, unsigned pitch_)
{
	data = data_;
	pitch = pitch_;
}

} // namespace openmsx
