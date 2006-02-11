// $Id$

#ifndef OUTPUTSURFACE_HH
#define OUTPUTSURFACE_HH

#include "noncopyable.hh"
#include <SDL.h>

namespace openmsx {

/** A frame buffer where pixels can be written to.
  * It could be an in-memory buffer or a video buffer visible to the user
  * (see VisibleSurface subclass).
  */
class OutputSurface : private noncopyable
{
public:
	virtual ~OutputSurface();

	unsigned getWidth() const  { return surface->w; }
	unsigned getHeight() const { return surface->h; }
	SDL_PixelFormat* getFormat() { return &format; }

	template <typename Pixel>
	Pixel* getLinePtr(unsigned y, Pixel* /*dummy*/) {
		return (Pixel*)(data + y * pitch);
	}

	unsigned mapRGB(double r, double g, double b);

protected:
	OutputSurface();

	SDL_Surface* surface;
	SDL_PixelFormat format;
	char* data;
	unsigned pitch;
};

} // namespace openmsx

#endif
