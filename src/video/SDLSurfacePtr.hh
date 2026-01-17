#ifndef SDLSURFACEPTR_HH
#define SDLSURFACEPTR_HH

#include "InitException.hh"

#include "MemBuffer.hh"
#include "narrow.hh"

#include <SDL.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>

/** Wrapper around a SDL_Surface.
 *
 * Makes sure SDL_FreeSurface() is called when this object goes out of scope.
 * It's modeled after std::unique_ptr, so it has the usual get(), reset() and
 * release() methods. Like unique_ptr it can be moved but not copied.
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
	  * throws: bad_alloc (no need to check for nullptr). */
	SDLSurfacePtr(unsigned width, unsigned height, unsigned depth,
	              Uint32 rMask, Uint32 gMask, Uint32 bMask, Uint32 aMask)
	{
		assert((depth % 8) == 0);
		unsigned pitch = width * (depth >> 3);
		unsigned size = height * pitch;
		buffer.resize(size);
		surface = SDL_CreateRGBSurfaceFrom(
			buffer.data(),
			narrow<int>(width), narrow<int>(height),
			narrow<int>(depth), narrow<int>(pitch),
			rMask, gMask, bMask, aMask);
		if (!surface) throw std::bad_alloc();
	}

	explicit SDLSurfacePtr(SDL_Surface* surface_ = nullptr)
		: surface(surface_)
	{
	}

	SDLSurfacePtr(SDLSurfacePtr&& other) noexcept
		: surface(other.surface)
		, buffer(std::move(other.buffer))
	{
		other.surface = nullptr;
	}

	SDLSurfacePtr(const SDLSurfacePtr&) = delete;
	SDLSurfacePtr& operator=(const SDLSurfacePtr&) = delete;

	~SDLSurfacePtr()
	{
		if (surface) SDL_FreeSurface(surface);
	}

	void reset(SDL_Surface* surface_ = nullptr)
	{
		SDLSurfacePtr temp(surface_);
		temp.swap(*this);
	}

	[[nodiscard]] SDL_Surface* get()
	{
		return surface;
	}
	[[nodiscard]] const SDL_Surface* get() const
	{
		return surface;
	}

	void swap(SDLSurfacePtr& other) noexcept
	{
		std::swap(surface, other.surface);
		std::swap(buffer,  other.buffer );
	}

	SDLSurfacePtr& operator=(SDLSurfacePtr&& other) noexcept
	{
		std::swap(surface, other.surface);
		std::swap(buffer,  other.buffer);
		return *this;
	}

	[[nodiscard]] SDL_Surface& operator*()
	{
		return *surface;
	}
	[[nodiscard]] const SDL_Surface& operator*() const
	{
		return *surface;
	}

	[[nodiscard]] SDL_Surface* operator->()
	{
		return surface;
	}
	[[nodiscard]] const SDL_Surface* operator->() const
	{
		return surface;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return get() != nullptr;
	}

	[[nodiscard]] void* getLinePtr(unsigned y)
	{
		assert(y < unsigned(surface->h));
		return static_cast<Uint8*>(surface->pixels) + y * surface->pitch;
	}
	[[nodiscard]] const void* getLinePtr(unsigned y) const
	{
		assert(y < unsigned(surface->h));
		return static_cast<const Uint8*>(surface->pixels) + y * surface->pitch;
	}

private:
	SDL_Surface* surface;
	openmsx::MemBuffer<char> buffer;
};


struct SDLDestroyTexture {
	static void operator()(SDL_Texture* t) { SDL_DestroyTexture(t); }
};
using SDLTexturePtr = std::unique_ptr<SDL_Texture, SDLDestroyTexture>;


struct SDLDestroyRenderer {
	static void operator()(SDL_Renderer* r) { SDL_DestroyRenderer(r); }
};
using SDLRendererPtr = std::unique_ptr<SDL_Renderer, SDLDestroyRenderer>;


struct SDLDestroyWindow {
	static void operator()(SDL_Window* w) { SDL_DestroyWindow(w); }
};
using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLDestroyWindow>;


struct SDLFreeFormat {
	static void operator()(SDL_PixelFormat* p) { SDL_FreeFormat(p); }
};
using SDLAllocFormatPtr = std::unique_ptr<SDL_PixelFormat, SDLFreeFormat>;


template<Uint32 FLAGS>
class SDLSubSystemInitializer
{
public:
	SDLSubSystemInitializer(const SDLSubSystemInitializer&) = delete;
	SDLSubSystemInitializer(SDLSubSystemInitializer&&) = delete;
	SDLSubSystemInitializer& operator=(const SDLSubSystemInitializer&) = delete;
	SDLSubSystemInitializer& operator=(SDLSubSystemInitializer&&) = delete;

	SDLSubSystemInitializer() {
		// SDL internally ref-counts sub-system initialization, so we
		// don't need to worry about it here.
		if (SDL_InitSubSystem(FLAGS) < 0) {
			throw openmsx::InitException(
				"SDL init failed (", FLAGS, "): ", SDL_GetError());
		}
	}
	~SDLSubSystemInitializer() {
		SDL_QuitSubSystem(FLAGS);
	}
};

#endif
