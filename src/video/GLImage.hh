// $Id$

#ifndef GLTEXTURE_HH
#define GLTEXTURE_HH

#include "BaseImage.hh"
#include "GLUtil.hh"
#include "openmsx.hh"
#include <string>

struct SDL_Surface;

namespace openmsx {

class OutputSurface;

class GLImage : public BaseImage
{
public:
	GLImage(OutputSurface& output, const std::string& filename);
	GLImage(OutputSurface& output, const std::string& filename,
	        double scaleFactor);
	GLImage(OutputSurface& output, const std::string& filename,
	        unsigned width, unsigned height);
	GLImage(OutputSurface& output,
	        unsigned width, unsigned height, byte alpha,
	        byte r = 0, byte g = 0, byte b = 0);
	GLImage(OutputSurface& output, SDL_Surface* image);
	virtual ~GLImage();

	virtual void draw(unsigned x, unsigned y, unsigned char alpha = 255);

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
