// $Id$

#ifndef SDLIMAGE_HH
#define SDLIMAGE_HH

#include "BaseImage.hh"
#include "SDLSurfacePtr.hh"
#include <string>

struct SDL_Surface;

namespace openmsx {

class SDLImage : public BaseImage
{
public:
	explicit SDLImage(const std::string& filename);
	explicit SDLImage(SDL_Surface* image);
	SDLImage(const std::string& filename, double scaleFactor);
	SDLImage(const std::string& filename, int width, int height);
	SDLImage(int width, int height, byte alpha,
	         byte r = 0, byte g = 0, byte b = 0);
	virtual ~SDLImage();

	virtual void draw(OutputSurface& output, int x, int y,
	                  byte alpha = 255);
	virtual int getWidth() const;
	virtual int getHeight() const;

private:
	void allocateWorkImage();

	SDLSurfacePtr image;
	SDLSurfacePtr workImage;
	int a;
	bool flipX, flipY;
};

} // namespace openmsx

#endif
