// $Id$

#include "GLImage.hh"
#include "MSXException.hh"
#include "SDLImage.hh"
#include <SDL_image.h>
#include <SDL.h>

using std::string;

namespace openmsx {

GLImage::GLImage(SDL_Surface* /*output*/, const string& filename)
{
	texture = loadTexture(filename, width, height, texCoord);
}

GLImage::~GLImage()
{
	glDeleteTextures(1, &texture);
}

void GLImage::draw(unsigned x, unsigned y, unsigned char alpha)
{
	glPushAttrib(GL_ENABLE_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
	glPopAttrib();
}


static int powerOfTwo(int a)
{
	int res = 1;
	while (a > res) {
		res <<= 1;
	}
	return res;
}

GLuint GLImage::loadTexture(const string& filename,
	unsigned& width, unsigned& height, GLfloat* texCoord)
{
	SDL_Surface* image1 = SDLImage::readImage(filename);
	if (image1 == NULL) {
		throw MSXException("Error loading image " + filename);
	}

	width  = image1->w;
	height = image1->h;
	int w2 = powerOfTwo(width);
	int h2 = powerOfTwo(height);
	texCoord[0] = 0.0f;			// Min X
	texCoord[1] = 0.0f;			// Min Y
	texCoord[2] = (GLfloat)width  / w2;	// Max X
	texCoord[3] = (GLfloat)height / h2;	// Max Y

	SDL_Surface* image2 = SDL_CreateRGBSurface(SDL_SWSURFACE, w2, h2, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
		0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
	);
	if (image2 == NULL) {
		SDL_FreeSurface(image1);
		throw MSXException("Error loading image " + filename);
	}

	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = width;
	area.h = height;
	SDL_SetAlpha(image1, 0, 0);
	SDL_BlitSurface(image1, &area, image2, &area);
	SDL_FreeSurface(image1);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, w2, h2, 0, GL_RGBA, GL_UNSIGNED_BYTE, image2->pixels);
	SDL_FreeSurface(image2);
	return texture;
}

} // namespace openmsx
