// $Id$

#ifndef SDLSURFACEPTR
#define SDLSURFACEPTR

#include <SDL.h>
#include <algorithm>
#include <new>
#include <cassert>
#include <cstdlib>

// This is a helper class, you shouldn't use it directly.
struct SDLSurfaceRef
{
	SDLSurfaceRef(SDL_Surface* surface_, void* buffer_)
		: surface(surface_), buffer(buffer_) {}
	SDL_Surface* surface;
	void* buffer;
};


/** Wrapper around a SDL_Surface.
 *
 * Makes sure SDL_FreeSurface() is called when this object goes out of scope.
 * It's modeled after std::auto_ptr, so it has the usual get(), reset() and
 * operator*() methods. It also has the same 'weird' copy and assignment
 * semantics.
 *
 * In addition to the SDL_Surface pointer, this wrapper also (optionally)
 * manages an extra memory buffer. Normally SDL_CreateRGBSurface() will
 * allocate/free an internal memory buffer for the surface. On construction
 * of the surface this buffer will be zero-initialized. Though in many cases
 * the surface will immediately be overwritten (so zero-initialization is
 * only extra overhead). It's possible to avoid this by creating the surface
 * using SDL_CreateRGBSurfaceFrom(). Though the downside of this is that you
 * have to manage the lifetime of the memory buffer yourself. And that's
 * exactly what this wrapper can do.
 *
 * As a bonus this wrapper has a getLinePtr() method to hide some of the
 * casting. But apart from this it doesn't try to abstract any SDL
 * functionality.
 */
class SDLSurfacePtr
{
public:
	/** Create a (software) surface with uninitialized pixel content.
	  * throws: bad_alloc (no need to check for NULL pointer). */
	SDLSurfacePtr(unsigned width, unsigned height, unsigned depth,
	              Uint32 rMask, Uint32 gMask, Uint32 bMask, Uint32 aMask)
	{
		assert((depth % 8) == 0);
		unsigned pitch = width * (depth >> 3);
		unsigned size = height * pitch;
		buffer = malloc(size);
		if (!buffer) throw std::bad_alloc();
		surface = SDL_CreateRGBSurfaceFrom(
			buffer, width, height, depth, pitch,
			rMask, gMask, bMask, aMask);
		if (!surface) {
			free(buffer);
			throw std::bad_alloc();
		}
	}

	SDLSurfacePtr(SDL_Surface* surface_ = NULL, void* buffer_ = NULL)
		: surface(surface_)
		, buffer(buffer_)
	{
	}

	SDLSurfacePtr(SDLSurfacePtr& other)
		: surface(other.surface)
		, buffer(other.buffer)
	{
		// Like std::auto_ptr, making a copy 'steals' the content
		// of the original.
		other.surface = NULL;
		other.buffer = NULL;
	}

	~SDLSurfacePtr()
	{
		if (surface) {
			SDL_FreeSurface(surface);
		}
		free(buffer);
	}

	void reset(SDL_Surface* surface_ = 0)
	{
		SDLSurfacePtr temp(surface_);
		temp.swap(*this);
	}

	SDL_Surface* get()
	{
		return surface;
	}
	const SDL_Surface* get() const
	{
		return surface;
	}

	void swap(SDLSurfacePtr& other)
	{
		std::swap(surface, other.surface);
		std::swap(buffer,  other.buffer );
	}

	SDLSurfacePtr& operator=(SDLSurfacePtr& other)
	{
		SDLSurfacePtr temp(other); // this 'steals' the content of 'other'
		temp.swap(*this);
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
		, buffer(ref.buffer)
	{
	}
	SDLSurfacePtr& operator=(SDLSurfaceRef ref)
	{
		if (ref.surface != surface) {
			assert(!buffer || (ref.buffer != buffer));
			SDLSurfacePtr temp(ref.surface, ref.buffer);
			temp.swap(*this);
		} else {
			assert(ref.buffer == buffer);
		}
		return *this;
	}
	operator SDLSurfaceRef()
	{
		SDLSurfaceRef ref(surface, buffer);
		surface = NULL;
		buffer = NULL;
		return ref;
	}

private:
	SDL_Surface* surface;
	void* buffer;
};

#endif
