// $Id$

#ifndef GLTEXTURE_HH
#define GLTEXTURE_HH

#include "BaseImage.hh"
#include "SDLSurfacePtr.hh"
#include "GLUtil.hh"
#include "openmsx.hh"
#include <string>

namespace openmsx {

class GLImage : public BaseImage
{
public:
	explicit GLImage(const std::string& filename);
	explicit GLImage(SDLSurfacePtr image);
	GLImage(const std::string& filename, double scaleFactor);
	GLImage(const std::string& filename, int width, int height);
	GLImage(int width, int height, unsigned rgba);
	GLImage(int width, int height, const unsigned* rgba);
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
	int a[4];
	byte r[4], g[4], b[4];
};

} // namespace openmsx

#endif
