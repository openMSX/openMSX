// $Id$

#include "RawFrame.hh"

namespace openmsx {

RawFrame::RawFrame(SDL_PixelFormat* format, unsigned maxWidth)
{
	surface = SDL_CreateRGBSurface(
		SDL_SWSURFACE,
		maxWidth, HEIGHT,
		format->BitsPerPixel,
		format->Rmask,
		format->Gmask,
		format->Bmask,
		format->Amask
		);
	reinit(FIELD_NONINTERLACED);
}

RawFrame::~RawFrame()
{
	SDL_FreeSurface(surface);
}

void RawFrame::reinit(FieldType field)
{
	this->field = field;

	// Initialise lineWidth.
	// TODO: Colour of blank lines should be initialised as well.
	// TODO: Is there a point to initialising this at all?
	for (unsigned y = 0; y < HEIGHT; y++) {
		lineWidth[y] = 0;
	}
}

} // namespace openmsx
