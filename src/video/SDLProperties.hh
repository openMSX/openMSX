#ifndef SDLPROPERTIES_HH
#define SDLPROPERTIES_HH

#include <SDL3/SDL.h>

#include <new>

/** Wrapper around a SDL property group (SDL_PropertiesID).
 *
 * Creates the group on construction and makes sure SDL_DestroyProperties() is
 * called when this object goes out of scope. It can't be copied or moved. Use
 * get() to pass it to the SDL functions that set or use the properties. Apart
 * from this it doesn't try to abstract any SDL functionality.
 */
class SDLProperties
{
public:
	/** throws: bad_alloc (no need to check for an invalid id). */
	SDLProperties()
		: props(SDL_CreateProperties())
	{
		if (!props) throw std::bad_alloc();
	}

	SDLProperties(const SDLProperties&) = delete;
	SDLProperties(SDLProperties&&) = delete;
	SDLProperties& operator=(const SDLProperties&) = delete;
	SDLProperties& operator=(SDLProperties&&) = delete;

	~SDLProperties()
	{
		SDL_DestroyProperties(props);
	}

	[[nodiscard]] SDL_PropertiesID get() const
	{
		return props;
	}

private:
	SDL_PropertiesID props;
};

#endif
