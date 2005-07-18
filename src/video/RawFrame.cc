// $Id$

#include "RawFrame.hh"


namespace openmsx {

RawFrame::RawFrame(SDL_PixelFormat* format, bool oddField)
{
	surface = SDL_CreateRGBSurface(
		SDL_SWSURFACE,
		WIDTH, HEIGHT,
		format->BitsPerPixel,
		format->Rmask,
		format->Gmask,
		format->Bmask,
		format->Amask
		);
	reinit(oddField);
}

RawFrame::~RawFrame()
{
	SDL_FreeSurface(surface);
}

void RawFrame::reinit(bool oddField)
{
	this->oddField = oddField;

	// Initialise lineContent.
	// TODO: Colour of blank lines should be initialised as well.
	// TODO: Is there a point to initialising this at all?
	for (unsigned y = 0; y < HEIGHT; y++) {
		lineContent[y] = LINE_BLANK;
	}
}

} // namespace openmsx
