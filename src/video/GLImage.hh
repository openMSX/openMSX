// $Id$

#ifndef _GLTEXTURE_HH_
#define _GLTEXTURE_HH_

#include <string>
#include "GLUtil.hh"

class SDL_Surface;

namespace openmsx {

class GLImage
{
public:
	GLImage(SDL_Surface* output, const std::string& filename);
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
