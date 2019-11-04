#ifndef SDLOUTPUTSURFACE_HH
#define SDLOUTPUTSURFACE_HH

#include "OutputSurface.hh"
#include "SDLPixelFormat.hh"
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

	const PixelFormat& getPixelFormat() const override { return pixelFormat; }
	SDL_Surface*  getSDLSurface()  const { return surface; }
	SDL_Renderer* getSDLRenderer() const { return renderer; }

	/** Same as mapRGB, but RGB components are in range [0..255].
	 */
	unsigned mapRGB255(gl::ivec3 rgb) override
	{
		return pixelFormat.map(rgb[0], rgb[1], rgb[2]); // alpha is fully opaque
	}

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
	SDLOutputSurface(unsigned bpp,
	                 unsigned Rmask, unsigned Rshift, unsigned Rloss,
	                 unsigned Gmask, unsigned Gshift, unsigned Gloss,
	                 unsigned Bmask, unsigned Bshift, unsigned Bloss,
	                 unsigned Amask, unsigned Ashift, unsigned Aloss)
		: pixelFormat(bpp, Rmask, Rshift, Rloss,
		              Gmask, Gshift, Gloss,
		              Bmask, Bshift, Bloss,
		              Amask, Ashift, Aloss)
	{
	}
	SDLOutputSurface(const SDL_PixelFormat& format) : pixelFormat(format) {}

	void setSDLSurface(SDL_Surface* surface_) { surface = surface_; }
	void setSDLRenderer(SDL_Renderer* r) { renderer = r; }
	void setBufferPtr(char* data, unsigned pitch);

	SDLPixelFormat pixelFormat;

private:
	SDL_Surface* surface = nullptr;
	SDL_Renderer* renderer = nullptr;
	char* data;
	unsigned pitch;
	bool locked = false;
};

} // namespace openmsx

#endif
