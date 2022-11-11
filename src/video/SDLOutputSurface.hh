#ifndef SDLOUTPUTSURFACE_HH
#define SDLOUTPUTSURFACE_HH

#include "OutputSurface.hh"
#include <cassert>
#include <concepts>
#include <span>
#include <SDL.h>

namespace openmsx {

class SDLDirectPixelAccess
{
public:
	SDLDirectPixelAccess(SDL_Surface* surface_);
	~SDLDirectPixelAccess();

	template<std::unsigned_integral Pixel>
	[[nodiscard]] std::span<Pixel> getLine(unsigned y) {
		return {reinterpret_cast<Pixel*>(static_cast<char*>(surface->pixels) + y * surface->pitch),
		        size_t(surface->w)};
	}

private:
	SDL_Surface* surface;
};

/** A frame buffer where pixels can be written to.
  * It could be an in-memory buffer or a video buffer visible to the user
  * (see VisibleSurface subclass).
  */
class SDLOutputSurface : public OutputSurface
{
public:
	SDLOutputSurface(const SDLOutputSurface&) = delete;
	SDLOutputSurface& operator=(const SDLOutputSurface&) = delete;

	[[nodiscard]] SDL_Surface*  getSDLSurface()  const { return surface; }
	[[nodiscard]] SDL_Renderer* getSDLRenderer() const { return renderer; }

	/** Return a SDLDirectPixelAccess object. Via this object pointers to
	 * individual Pixel lines can be obtained. Those pointer only remain
	 * valid for as long as the SDLDirectPixelAccess object is kept alive.
	 * And that object should be kept for at most the duration of one
	 * frame. (This allows the implementation to lock/unlock the underlying
	 * SDL surface).
	 *
	 * Note that direct pixel access is not always supported (e.g. not for
	 * openGL based surfaces). TODO can we move this method down the class
	 * hierarchy so that it's only available on classes that do support it?
	 */
	[[nodiscard]] SDLDirectPixelAccess getDirectPixelAccess()
	{
		return {getSDLSurface()};
	}

	/** Copy frame buffer to display buffer.
	  * The default implementation does nothing.
	  */
	virtual void flushFrameBuffer() {}

	/** Clear frame buffer (paint it black).
	  * The default implementation does nothing.
	 */
	virtual void clearScreen() {}

protected:
	SDLOutputSurface() = default;

	void setSDLPixelFormat(const SDL_PixelFormat& format);
	void setSDLSurface(SDL_Surface* surface_) { surface = surface_; }
	void setSDLRenderer(SDL_Renderer* r) { renderer = r; }

private:
	SDL_Surface* surface = nullptr;
	SDL_Renderer* renderer = nullptr;
};

} // namespace openmsx

#endif
