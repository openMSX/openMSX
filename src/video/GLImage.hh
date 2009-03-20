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
	GLImage(const std::string& filename, unsigned width, unsigned height);
	GLImage(unsigned width, unsigned height,
	        byte alpha, byte r = 0, byte g = 0, byte b = 0);
	GLImage(SDL_Surface* image);
	virtual ~GLImage();

	virtual void draw(OutputSurface& output, unsigned x, unsigned y,
	                  byte alpha = 255);
	virtual unsigned getWidth() const;
	virtual unsigned getHeight() const;

private:
	GLuint texture;
	unsigned width;
	unsigned height;
	GLfloat texCoord[4];
	byte r, g, b;
	int a;

public:
	static GLuint loadTexture(const std::string& filename,
	                          unsigned& width, unsigned& height,
	                          GLfloat* texcoord);
	static GLuint loadTexture(SDL_Surface* surface,
	                          unsigned& width, unsigned& height,
	                          GLfloat* texCoord);
};

} // namespace openmsx

#endif
