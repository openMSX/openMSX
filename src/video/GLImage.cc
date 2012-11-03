// $Id$

#include "GLImage.hh"
#include "SDLSurfacePtr.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "PNG.hh"
#include "build-info.hh"
#include <cstdlib>
#include <SDL.h>

using std::string;

namespace openmsx {

static GLuint loadTexture(SDLSurfacePtr surface,
	int& width, int& height, GLfloat* texCoord)
{
	width  = surface->w;
	height = surface->h;
	int w2 = Math::powerOfTwo(width);
	int h2 = Math::powerOfTwo(height);
	texCoord[0] = 0.0f;                 // min X
	texCoord[1] = 0.0f;                 // min Y
	texCoord[2] = GLfloat(width)  / w2; // max X
	texCoord[3] = GLfloat(height) / h2; // max Y

	SDLSurfacePtr image2(w2, h2, 32,
		OPENMSX_BIGENDIAN ? 0xFF000000 : 0x000000FF,
		OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00,
		OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x00FF0000,
		OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000);

	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = width;
	area.h = height;
	SDL_SetAlpha(surface.get(), 0, 0);
	SDL_BlitSurface(surface.get(), &area, image2.get(), &area);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w2, h2, 0, GL_RGBA,
	             GL_UNSIGNED_BYTE, image2->pixels);
	return texture;
}

static GLuint loadTexture(const string& filename,
	int& width, int& height, GLfloat* texCoord)
{
	SDLSurfacePtr surface(PNG::load(filename, false));
	try {
		return loadTexture(std::move(surface), width, height, texCoord);
	} catch (MSXException& e) {
		throw MSXException("Error loading image " + filename +
		                   ": " + e.getMessage());
	}
}


GLImage::GLImage(const string& filename)
{
	texture = loadTexture(filename, width, height, texCoord);
}

GLImage::GLImage(const string& filename, double scalefactor)
{
	texture = loadTexture(filename, width, height, texCoord);
	width  = int(scalefactor * width);
	height = int(scalefactor * height);
	checkSize(width, height);
}

GLImage::GLImage(const string& filename, int width_, int height_)
{
	checkSize(width_, height_);
	texture = loadTexture(filename, width, height, texCoord);
	width  = width_;
	height = height_;
}

GLImage::GLImage(int width_, int height_, unsigned rgba)
{
	checkSize(width_, height_);
	texture = 0;
	width  = width_;
	height = height_;
	borderSize = 0;
	for (int i = 0; i < 4; ++i) {
		r[i] = (rgba >> 24) & 0xff;
		g[i] = (rgba >> 16) & 0xff;
		b[i] = (rgba >>  8) & 0xff;
		unsigned alpha = (rgba >> 0) & 0xff;
		a[i] = (alpha == 255) ? 256 : alpha;
	}
}

GLImage::GLImage(int width_, int height_, const unsigned* rgba,
                 int borderSize_, unsigned borderRGBA)
{
	checkSize(width_, height_);
	texture = 0;
	width  = width_;
	height = height_;
	borderSize = borderSize_;
	for (int i = 0; i < 4; ++i) {
		r[i] = (rgba[i] >> 24) & 0xff;
		g[i] = (rgba[i] >> 16) & 0xff;
		b[i] = (rgba[i] >>  8) & 0xff;
		unsigned alpha = (rgba[i] >> 0) & 0xff;
		a[i] = (alpha == 255) ? 256 : alpha;
	}

	borderR = (borderRGBA >> 24) & 0xff;
	borderG = (borderRGBA >> 16) & 0xff;
	borderB = (borderRGBA >>  8) & 0xff;
	unsigned alpha = (borderRGBA >> 0) & 0xff;
	borderA = (alpha == 255) ? 256 : alpha;
}

GLImage::GLImage(SDLSurfacePtr image)
{
	texture = loadTexture(std::move(image), width, height, texCoord);
}

GLImage::~GLImage()
{
	glDeleteTextures(1, &texture);
}

void GLImage::draw(OutputSurface& /*output*/, int x, int y, byte alpha)
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
		bool onlyBorder = (2 * borderSize >= abs(width )) ||
		                  (2 * borderSize >= abs(height));
		if (onlyBorder) {
			glBegin(GL_QUADS);
			glColor4ub(borderR, borderG, borderB,
			           (borderA * alpha) / 256);
			glVertex2i(x,         y         );
			glVertex2i(x,         y + height);
			glVertex2i(x + width, y + height);
			glVertex2i(x + width, y         );
			glEnd();
		} else {
			// interior
			int bx = (width  > 0) ? borderSize : -borderSize;
			int by = (height > 0) ? borderSize : -borderSize;
			glBegin(GL_QUADS);
			glColor4ub(r[0], g[0], b[0], (a[0] * alpha) / 256);
			glVertex2i(x + bx,         y + by        );
			glColor4ub(r[2], g[2], b[2], (a[2] * alpha) / 256);
			glVertex2i(x + bx,         y + height - by);
			glColor4ub(r[3], g[3], b[3], (a[3] * alpha) / 256);
			glVertex2i(x + width - bx, y + height - by);
			glColor4ub(r[1], g[1], b[1], (a[1] * alpha) / 256);
			glVertex2i(x + width - bx, y + by        );
			glEnd();
			if (borderSize > 0) {
				glColor4ub(borderR, borderG, borderB,
					   (borderA * alpha) / 256);
				// 0/8---------------6
				// |                 |
				// |   1/9-------7   |
				// |   |         |   |
				// |   |         |   |
				// |   3-------- 5   |
				// |                 |
				// 2-----------------4
				glBegin(GL_TRIANGLE_STRIP);
				glVertex2i(x             , y              ); // 0
				glVertex2i(x         + bx, y          + by); // 1
				glVertex2i(x             , y + height     ); // 2
				glVertex2i(x         + bx, y + height - by); // 3
				glVertex2i(x + width     , y + height     ); // 4
				glVertex2i(x + width - bx, y + height - by); // 5
				glVertex2i(x + width     , y              ); // 6
				glVertex2i(x + width - bx, y          + by); // 7
				glVertex2i(x             , y              ); // 8 = 0
				glVertex2i(x         + bx, y          + by); // 9 = 1
				glEnd();
			}
		}
	}
	glPopAttrib();
}

int GLImage::getWidth() const
{
	return width;
}

int GLImage::getHeight() const
{
	return height;
}

} // namespace openmsx
