// $Id$

#ifndef SDLIMAGE_HH
#define SDLIMAGE_HH

#include "BaseImage.hh"
#include <string>

struct SDL_Surface;

namespace openmsx {

class SDLImage : public BaseImage
{
public:
	SDLImage(const std::string& filename);
	SDLImage(const std::string& filename, double scaleFactor);
	SDLImage(const std::string& filename, unsigned width, unsigned height);
	SDLImage(unsigned width, unsigned height, byte alpha,
	         byte r = 0, byte g = 0, byte b = 0);
	SDLImage(SDL_Surface* image);
	virtual ~SDLImage();

	virtual void draw(OutputSurface& output, unsigned x, unsigned y,
	                  byte alpha = 255);
	virtual unsigned getWidth() const;
	virtual unsigned getHeight() const;

private:
	void allocateWorkImage();

	SDL_Surface* image;
	SDL_Surface* workImage;
	int a;

public:
	static SDL_Surface* readImage(const std::string& filename);
};

} // namespace openmsx

#endif
