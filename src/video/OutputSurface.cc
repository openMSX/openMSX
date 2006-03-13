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

unsigned OutputSurface::mapRGB(double dr, double dg, double db)
{
	int r = static_cast<int>(dr * 255.0);
	int g = static_cast<int>(dg * 255.0);
	int b = static_cast<int>(db * 255.0);
	return SDL_MapRGB(&format, r, g, b);
}

} // namespace openmsx
