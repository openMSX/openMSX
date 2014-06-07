#include "GLImage.hh"
#include "GLPrograms.hh"
#include "SDLSurfacePtr.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "PNG.hh"
#include "build-info.hh"
#include <cstdlib>
#include <SDL.h>

using std::string;
using namespace gl;

namespace openmsx {

static gl::Texture loadTexture(SDLSurfacePtr surface,
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

	gl::Texture texture(true); // enable interpolation
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w2, h2, 0, GL_RGBA,
	             GL_UNSIGNED_BYTE, image2->pixels);
	return texture;
}

static gl::Texture loadTexture(const string& filename,
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
	: texture(loadTexture(filename, width, height, texCoord))
{
}

GLImage::GLImage(const string& filename, double scalefactor)
	: texture(loadTexture(filename, width, height, texCoord))
{
	width  = int(scalefactor * width);
	height = int(scalefactor * height);
	checkSize(width, height);
}

GLImage::GLImage(const string& filename, int width_, int height_)
	: texture(loadTexture(filename, width, height, texCoord))
{
	checkSize(width_, height_);
	width  = width_;
	height = height_;
}

GLImage::GLImage(int width_, int height_, unsigned rgba)
	: texture(gl::Null())
{
	checkSize(width_, height_);
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
	: texture(gl::Null())
{
	checkSize(width_, height_);
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
	: texture(loadTexture(std::move(image), width, height, texCoord))
{
}

void GLImage::draw(OutputSurface& /*output*/, int x, int y, byte alpha)
{
	// 4-----------------7
	// |                 |
	// |   0---------3   |
	// |   |         |   |
	// |   |         |   |
	// |   1-------- 2   |
	// |                 |
	// 5-----------------6
	int bx = (width  > 0) ? borderSize : -borderSize;
	int by = (height > 0) ? borderSize : -borderSize;
	vec2 pos[8] = {
		vec2(x         + bx, y          + by), // 0
		vec2(x         + bx, y + height - by), // 1
		vec2(x + width - bx, y + height - by), // 2
		vec2(x + width - bx, y          + by), // 3
		vec2(x             , y              ), // 4
		vec2(x             , y + height     ), // 5
		vec2(x + width     , y + height     ), // 6
		vec2(x + width     , y              ), // 7
	};

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnableVertexAttribArray(0);

	if (texture.get()) {
		vec2 tex[4] = {
			vec2(texCoord[0], texCoord[1]),
			vec2(texCoord[0], texCoord[3]),
			vec2(texCoord[2], texCoord[3]),
			vec2(texCoord[2], texCoord[1]),
		};

		progTex.activate();
		glUniform4f(unifTexColor, 1.0f, 1.0f, 1.0f, alpha / 255.0f);
		glUniformMatrix4fv(unifTexMvp, 1, GL_FALSE, &pixelMvp[0][0]);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos + 4);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex);
		glEnableVertexAttribArray(1);
		texture.bind();
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	} else {
		progFill.activate();
		glUniformMatrix4fv(unifTexMvp, 1, GL_FALSE, &pixelMvp[0][0]);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
		glVertexAttrib4f(1, borderR / 255.0f, borderG / 255.0f, borderB / 255.0f,
		                (borderA * alpha) / (255.0f * 255.0f));

		if ((2 * borderSize >= abs(width )) ||
		    (2 * borderSize >= abs(height))) {
			// only border
			glDisableVertexAttribArray(1);
			glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
		} else {
			// interior
			byte col[4][4] = {
				{ r[0], g[0], b[0], byte((a[0] * alpha) / 256) },
				{ r[2], g[2], b[2], byte((a[2] * alpha) / 256) },
				{ r[3], g[3], b[3], byte((a[3] * alpha) / 256) },
				{ r[1], g[1], b[1], byte((a[1] * alpha) / 256) },
			};
			glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, col);
			glEnableVertexAttribArray(1);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			// border
			if (borderSize > 0) {
				byte indices[10] = { 4,0,5,1,6,2,7,3,4,0 };
				glDisableVertexAttribArray(1);
				glDrawElements(GL_TRIANGLE_STRIP, 10, GL_UNSIGNED_BYTE, indices);
			}
		}
	}
	glDisable(GL_BLEND);
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
