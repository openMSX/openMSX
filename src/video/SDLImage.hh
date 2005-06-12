// $Id$

#ifndef SDLTEXTURE_HH
#define SDLTEXTURE_HH

#include "openmsx.hh"
#include <string>

class SDL_Surface;

namespace openmsx {

class SDLImage
{
public:
	SDLImage(SDL_Surface* output, const std::string& filename);
	SDLImage(SDL_Surface* output, const std::string& filename,
	         unsigned width, unsigned height, byte defaultAlpha = 255);
	SDLImage(SDL_Surface* output, unsigned width, unsigned height,
	         byte defaultAlpha = 255);
	~SDLImage();

	void draw(unsigned x, unsigned y, byte alpha = 255);

private:
	void init(const std::string& filename);

	SDL_Surface* outputScreen;
	SDL_Surface* image;
	SDL_Surface* workImage;

public:
	static SDL_Surface* loadImage(
		const std::string& filename,
		byte defaultAlpha = 255);
	static SDL_Surface* loadImage(
		const std::string& filename,
		unsigned width, unsigned height,
		byte defaultAlpha = 255);
	static SDL_Surface* readImage(const std::string& filename);
	static SDL_Surface* scaleImage32(
		SDL_Surface* input,
		unsigned width, unsigned height);
	static SDL_Surface* convertToDisplayFormat(
		SDL_Surface* input, byte defaultAlpha);
	static int zoomSurface(SDL_Surface* src, SDL_Surface* dst, bool smooth);
};

} // namespace openmsx

#endif
