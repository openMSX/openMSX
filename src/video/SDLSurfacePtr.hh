// $Id$

#ifndef SDLSURFACEPTR
#define SDLSURFACEPTR

#include <SDL.h>
#include <algorithm>
#include <cassert>

/** Wrapper around a SDL_Surface.
 * Makes sure SDL_FreeSurface() is called when this object goes out of scope.
 * It's modeled after std::auto_ptr, so it has the usual get(), reset() and
 * release() methods. It also has the same copy and assignment properties.
 *
 * As a bonus it has a getLinePtr() method to hide some of the casting. But
 * apart from this it doesn't try to abstract any SDL functionality.
 */
class SDLSurfacePtr
{
public:
	SDLSurfacePtr(SDL_Surface* surface_ = 0)
		: surface(surface_)
	{
	}

	SDLSurfacePtr(SDLSurfacePtr& other)
		: surface(other.surface)
	{
		other.surface = 0;
	}

	~SDLSurfacePtr()
	{
		if (surface) {
			SDL_FreeSurface(surface);
		}
	}

	void reset(SDL_Surface* surface_ = 0)
	{
		SDLSurfacePtr(surface_).swap(*this);
	}

	SDL_Surface* get()
	{
		return surface;
	}

	SDL_Surface* release()
	{
		SDL_Surface* result = surface;
		surface = 0;
		return result;
	}

	void swap(SDLSurfacePtr& other)
	{
		std::swap(surface, other.surface);
	}

	SDLSurfacePtr& operator=(SDLSurfacePtr& other)
	{
		reset(other.release());
		return *this;
	}

	SDL_Surface& operator*()
	{
		return *surface;
	}

	SDL_Surface* operator->()
	{
		return surface;
	}

	void* getLinePtr(unsigned y)
	{
		assert(y < unsigned(surface->h));
		return static_cast<Uint8*>(surface->pixels) + y * surface->pitch;
	}

private:
	SDL_Surface* surface;
};

#endif
