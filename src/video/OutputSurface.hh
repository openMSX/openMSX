// $Id$

#ifndef OUTPUTSURFACE_HH
#define OUTPUTSURFACE_HH

#include "noncopyable.hh"
#include <SDL.h>
#include <cassert>

namespace openmsx {

/** A frame buffer where pixels can be written to.
  * It could be an in-memory buffer or a video buffer visible to the user
  * (see VisibleSurface subclass).
  */
class OutputSurface : private noncopyable
{
public:
	virtual ~OutputSurface();

	unsigned getWidth() const  { return displaySurface->w; }
	unsigned getHeight() const { return displaySurface->h; }
	SDL_PixelFormat& getSDLFormat() { return format; }

	/** Lock this OutputSurface.
	  * Direct pixel access is only allowed on a locked surface.
	  * Locking an already locked surface has no effect.
	  */
	void lock();

	/** Unlock this OutputSurface.
	  * @see lock().
	  */
	void unlock();

	/** Is this OutputSurface currently locked?
	  */
	bool isLocked() const { return locked; }

	template <typename Pixel>
	Pixel* getLinePtrDirect(unsigned y) {
		assert(isLocked());
		return reinterpret_cast<Pixel*>(data + y * pitch);
	}

	virtual unsigned mapRGB(double dr, double dg, double db);

	SDL_Surface* getSDLDisplaySurface() const { return displaySurface; }
	SDL_Surface* getSDLWorkSurface()    const { return workSurface; }

protected:
	OutputSurface();
	void setSDLDisplaySurface(SDL_Surface* surface);
	void setSDLWorkSurface   (SDL_Surface* surface);
	void setBufferPtr(char* data, unsigned pitch);

private:
	SDL_Surface* displaySurface;
	SDL_Surface* workSurface;
	SDL_PixelFormat format;
	char* data;
	unsigned pitch;
	bool locked;
};

} // namespace openmsx

#endif
