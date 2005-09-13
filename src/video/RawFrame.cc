// $Id$

#include "RawFrame.hh"

namespace openmsx {

static const unsigned WIDTH_UNINITIALIZED = 13;

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
	init(FIELD_UNINITIALIZED);
}

RawFrame::~RawFrame()
{
	SDL_FreeSurface(surface);
}

RawFrame::FieldType RawFrame::getField()
{
	assert(field != -1);
	return field;
}

unsigned RawFrame::getLineWidth(unsigned line)
{
	assert(line < HEIGHT);
	unsigned width = lineWidth[line];
	assert(width != WIDTH_UNINITIALIZED);
	return width;
}

void RawFrame::init(FieldType field)
{
	this->field = field;

	// Mark lineWidth as uninitialized.
	for (unsigned y = 0; y < HEIGHT; y++) {
		lineWidth[y] = WIDTH_UNINITIALIZED;
	}
}

} // namespace openmsx
