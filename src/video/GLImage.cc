// $Id$

#include "GLImage.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "SDLImage.hh"
#include <SDL.h>

using std::string;

namespace openmsx {

GLImage::GLImage(const string& filename)
{
	texture = loadTexture(filename, width, height, texCoord);
}

GLImage::GLImage(const string& filename, double scalefactor)
{
	texture = loadTexture(filename, width, height, texCoord);
	width  = unsigned(scalefactor * width);
	height = unsigned(scalefactor * height);
}

GLImage::GLImage(const string& filename, unsigned width_, unsigned height_)
{
	texture = loadTexture(filename, width, height, texCoord);
	width  = width_;
	height = height_;
}

GLImage::GLImage(unsigned width_, unsigned height_,
                 byte alpha, byte r_, byte g_, byte b_)
{
	texture = 0;
	width  = width_;
	height = height_;
	r = r_;
	g = g_;
	b = b_;
	a = (alpha == 255) ? 256 : alpha;
}

GLImage::GLImage(SDL_Surface* image)
{
	texture = loadTexture(image, width, height, texCoord);
	SDL_FreeSurface(image);
}

GLImage::~GLImage()
{
	glDeleteTextures(1, &texture);
}

void GLImage::draw(OutputSurface& /*output*/, unsigned x, unsigned y,
                   unsigned char alpha)
{
	glPushAttrib(GL_ENABLE_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (texture) {
		glEnable(GL_TEXTURE_2D);
		glColor4ub(255, 255, 255, alpha);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
			  (alpha == 255) ? GL_REPLACE : GL_MODULATE);
		glBegin(GL_QUADS);
		glTexCoord2f(texCoord[0], texCoord[1]); glVertex2i(x,         y         );
		glTexCoord2f(texCoord[0], texCoord[3]); glVertex2i(x,         y + height);
		glTexCoord2f(texCoord[2], texCoord[3]); glVertex2i(x + width, y + height);
		glTexCoord2f(texCoord[2], texCoord[1]); glVertex2i(x + width, y         );
		glEnd();
	} else {
		glColor4ub(r, g, b, (a * alpha) / 256);
		glBegin(GL_QUADS);
		glVertex2i(x,         y         );
		glVertex2i(x,         y + height);
		glVertex2i(x + width, y + height);
		glVertex2i(x + width, y         );
		glEnd();
	}
	glPopAttrib();
}

unsigned GLImage::getWidth() const
{
	return width;
}

unsigned GLImage::getHeight() const
{
	return height;
}

GLuint GLImage::loadTexture(const string& filename,
	unsigned& width, unsigned& height, GLfloat* texCoord)
{
	SDL_Surface* surface = SDLImage::readImage(filename);
	if (surface == NULL) {
		throw MSXException("Error loading image " + filename);
	}
	GLuint result;
	try {
		result = loadTexture(surface, width, height, texCoord);
	} catch (MSXException& e) {
		SDL_FreeSurface(surface);
		throw MSXException("Error loading image " + filename +
		                   ": " + e.getMessage());
	}
	SDL_FreeSurface(surface);
	return result;
}

GLuint GLImage::loadTexture(SDL_Surface* surface,
	unsigned& width, unsigned& height, GLfloat* texCoord)
{
	width  = surface->w;
	height = surface->h;
	int w2 = Math::powerOfTwo(width);
	int h2 = Math::powerOfTwo(height);
	texCoord[0] = 0.0f;                 // min X
	texCoord[1] = 0.0f;                 // min Y
	texCoord[2] = GLfloat(width)  / w2; // max X
	texCoord[3] = GLfloat(height) / h2; // max Y

	SDL_Surface* image2 = SDL_CreateRGBSurface(SDL_SWSURFACE, w2, h2, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
		0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
	);
	if (image2 == NULL) {
		throw MSXException("Couldn't allocate surface");
	}

	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = width;
	area.h = height;
	SDL_SetAlpha(surface, 0, 0);
	SDL_BlitSurface(surface, &area, image2, &area);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w2, h2, 0, GL_RGBA, GL_UNSIGNED_BYTE, image2->pixels);
	SDL_FreeSurface(image2);
	return texture;
}

} // namespace openmsx
