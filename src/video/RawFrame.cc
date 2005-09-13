// $Id$

#include "RawFrame.hh"
// For definition of "byte", remove later?
#include "openmsx.hh"
#include <cstring>


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

	// Start with a black frame.
	init(FIELD_NONINTERLACED);
	for (unsigned line = 0; line < HEIGHT; line++) {
		// Make first two pixels black.
		memset(getLinePtrImpl(line), 0, format->BitsPerPixel * 2);
		// Mark line as border.
		lineWidth[line] = 0;
	}
}

RawFrame::~RawFrame()
{
	SDL_FreeSurface(surface);
}

RawFrame::FieldType RawFrame::getField()
{
	return field;
}

unsigned RawFrame::getLineWidth(unsigned line)
{
	assert(line < HEIGHT);
	return lineWidth[line];
}

void RawFrame::init(FieldType field)
{
	this->field = field;
}

void* RawFrame::getLinePtrImpl(unsigned line) {
	return reinterpret_cast<byte*>(surface->pixels) + line * surface->pitch;
}

} // namespace openmsx
