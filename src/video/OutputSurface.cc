// $Id$

#include "OutputSurface.hh"

namespace openmsx {

OutputSurface::OutputSurface()
	: surface(0)
{
}

OutputSurface::~OutputSurface()
{
}

unsigned OutputSurface::mapRGB(double r, double g, double b)
{
	return SDL_MapRGB(&format, int(r * 255), int(g * 255), int(b * 255));
}

} // namespace openmsx
