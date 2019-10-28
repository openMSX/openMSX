#ifndef SDLOUTPUTSURFACE_HH
#define SDLOUTPUTSURFACE_HH

#include "OutputSurface.hh"
#include "gl_vec.hh"
#include <cassert>
#include <SDL.h>

namespace openmsx {

/** A frame buffer where pixels can be written to.
  * It could be an in-memory buffer or a video buffer visible to the user
  * (see VisibleSurface subclass).
  */
class SDLOutputSurface : public OutputSurface
{
public:
	SDLOutputSurface(const SDLOutputSurface&) = delete;
	SDLOutputSurface& operator=(const SDLOutputSurface&) = delete;

	int getWidth()  const override { return surface->w; }
	int getHeight() const override { return surface->h; }

	SDL_Surface*  getSDLSurface()  const { return surface; }
	SDL_Renderer* getSDLRenderer() const { return renderer; }

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

	/** Returns a pointer to the requested line in the pixel buffer.
	  */
	template <typename Pixel>
	Pixel* getLinePtrDirect(unsigned y) {
		assert(isLocked());
		return reinterpret_cast<Pixel*>(data + y * pitch);
	}

protected:
	SDLOutputSurface() = default;

	void setSDLSurface(SDL_Surface* surface_) { surface = surface_; }
	void setSDLRenderer(SDL_Renderer* r) { renderer = r; }
	void setBufferPtr(char* data, unsigned pitch);

private:
	SDL_Surface* surface = nullptr;
	SDL_Renderer* renderer = nullptr;
	char* data;
	unsigned pitch;
	bool locked = false;
};

} // namespace openmsx

#endif
