// $Id$

#ifndef GLTEXTURE_HH
#define GLTEXTURE_HH

#include "GLUtil.hh"
#include "noncopyable.hh"
#include <string>

class SDL_Surface;

namespace openmsx {

class GLImage : private noncopyable
{
public:
	GLImage(SDL_Surface* output, const std::string& filename);
	GLImage(SDL_Surface* output, const std::string& filename,
	        double scaleFactor);
	~GLImage();

	void draw(unsigned x, unsigned y, unsigned char alpha = 255);

private:
	GLuint texture;
	unsigned width;
	unsigned height;
	GLfloat texCoord[4];

public:
	static GLuint loadTexture(const std::string& filename,
	                          unsigned& width, unsigned& height,
	                          GLfloat* texCoord);
};

} // namespace openmsx

#endif
