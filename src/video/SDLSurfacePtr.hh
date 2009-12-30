// $Id$

#ifndef SDLSURFACEPTR
#define SDLSURFACEPTR

#include <SDL.h>
#include <algorithm>
#include <cassert>

// This is a helper class, you shouldn't use it directly.
struct SDLSurfaceRef
{
	explicit SDLSurfaceRef(SDL_Surface* surface_)
		: surface(surface_) {}
	SDL_Surface* surface;
};


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
		: surface(other.release())
	{
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
	const SDL_Surface* get() const
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
	const SDL_Surface& operator*() const
	{
		return *surface;
	}

	SDL_Surface* operator->()
	{
		return surface;
	}
	const SDL_Surface* operator->() const
	{
		return surface;
	}

	void* getLinePtr(unsigned y)
	{
		assert(y < unsigned(surface->h));
		return static_cast<Uint8*>(surface->pixels) + y * surface->pitch;
	}
	const void* getLinePtr(unsigned y) const
	{
		return const_cast<SDLSurfacePtr*>(this)->getLinePtr(y);
	}

	// Automatic conversions. This is required to allow constructs like
	//   SDLSurfacePtr myFunction();
	//   ..
	//   SDLSurfacePtr myPtr = myFunction();
	// Trick copied from gcc's auto_ptr implementation.
	SDLSurfacePtr(SDLSurfaceRef ref)
		: surface(ref.surface)
	{
	}
	SDLSurfacePtr& operator=(SDLSurfaceRef ref)
	{
		if (ref.surface != surface) {
			reset(ref.surface);
		}
		return *this;
	}
	operator SDLSurfaceRef()
	{
		return SDLSurfaceRef(release());
	}

private:
	SDL_Surface* surface;
};

#endif
