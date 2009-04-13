// $Id$

#ifndef GLTEXTURE_HH
#define GLTEXTURE_HH

#include "BaseImage.hh"
#include "GLUtil.hh"
#include "openmsx.hh"
#include <string>

struct SDL_Surface;

namespace openmsx {

class GLImage : public BaseImage
{
public:
	GLImage(const std::string& filename);
	GLImage(const std::string& filename, double scaleFactor);
	GLImage(const std::string& filename, int width, int height);
	GLImage(int width, int height,
	        byte alpha, byte r = 0, byte g = 0, byte b = 0);
	GLImage(SDL_Surface* image);
	virtual ~GLImage();

	virtual void draw(OutputSurface& output, int x, int y,
	                  byte alpha = 255);
	virtual int getWidth() const;
	virtual int getHeight() const;

private:
	GLuint texture;
	unsigned width;
	unsigned height;
	GLfloat texCoord[4];
	byte r, g, b;
	int a;
};

} // namespace openmsx

#endif
