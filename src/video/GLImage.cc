#include "GLImage.hh"
#include "GLContext.hh"
#include "SDLSurfacePtr.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "PNG.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <cstdlib>
#include <SDL.h>

using namespace gl;

namespace openmsx {

static gl::Texture loadTexture(
	SDLSurfacePtr surface, ivec2& size)
{
	size = ivec2(surface->w, surface->h);
	// Make a copy to convert to the correct pixel format.
	// TODO instead directly load the image in the correct format.
	SDLSurfacePtr image2(size[0], size[1], 32,
		OPENMSX_BIGENDIAN ? 0xFF000000 : 0x000000FF,
		OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00,
		OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x00FF0000,
		OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000);

	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = size[0];
	area.h = size[1];
	SDL_SetSurfaceBlendMode(surface.get(), SDL_BLENDMODE_NONE);
	SDL_BlitSurface(surface.get(), &area, image2.get(), &area);

	gl::Texture texture(true); // enable interpolation
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size[0], size[1], 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, image2->pixels);
	return texture;
}

static gl::Texture loadTexture(
	const std::string& filename, ivec2& size)
{
	SDLSurfacePtr surface(PNG::load(filename, false));
	try {
		return loadTexture(std::move(surface), size);
	} catch (MSXException& e) {
		throw MSXException("Error loading image ", filename, ": ",
		                   e.getMessage());
	}
}


GLImage::GLImage(OutputSurface& /*output*/, const std::string& filename)
	: texture(loadTexture(filename, size))
{
}

GLImage::GLImage(OutputSurface& /*output*/, const std::string& filename, float scalefactor)
	: texture(loadTexture(filename, size))
{
	size = trunc(vec2(size) * scalefactor);
	checkSize(size);
}

GLImage::GLImage(OutputSurface& /*output*/, const std::string& filename, ivec2 size_)
	: texture(loadTexture(filename, size))
{
	checkSize(size_);
	size = size_;
}

GLImage::GLImage(OutputSurface& /*output*/, ivec2 size_, unsigned rgba)
	: texture(gl::Null())
{
	checkSize(size_);
	size = size_;
	borderSize = 0;
	borderR = borderG = borderB = borderA = 0; // not used, but avoid (harmless) UMR
	for (auto i : xrange(4)) {
		bgR[i] = (rgba >> 24) & 0xff;
		bgG[i] = (rgba >> 16) & 0xff;
		bgB[i] = (rgba >>  8) & 0xff;
		unsigned alpha = (rgba >> 0) & 0xff;
		bgA[i] = (alpha == 255) ? 256 : alpha;
	}
	initBuffers();
}

GLImage::GLImage(OutputSurface& /*output*/, ivec2 size_, span<const unsigned, 4> rgba,
                 int borderSize_, unsigned borderRGBA)
	: texture(gl::Null())
{
	checkSize(size_);
	size = size_;
	borderSize = borderSize_;
	for (auto i : xrange(4)) {
		bgR[i] = (rgba[i] >> 24) & 0xff;
		bgG[i] = (rgba[i] >> 16) & 0xff;
		bgB[i] = (rgba[i] >>  8) & 0xff;
		unsigned alpha = (rgba[i] >> 0) & 0xff;
		bgA[i] = (alpha == 255) ? 256 : alpha;
	}

	borderR = (borderRGBA >> 24) & 0xff;
	borderG = (borderRGBA >> 16) & 0xff;
	borderB = (borderRGBA >>  8) & 0xff;
	unsigned alpha = (borderRGBA >> 0) & 0xff;
	borderA = (alpha == 255) ? 256 : alpha;

	initBuffers();
}

GLImage::GLImage(OutputSurface& /*output*/, SDLSurfacePtr image)
	: texture(loadTexture(std::move(image), size))
{
}

void GLImage::initBuffers()
{
	// border
	uint8_t indices[10] = {4, 0, 5, 1, 6, 2, 7, 3, 4, 0};
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementsBuffer.get());
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GLImage::draw(OutputSurface& /*output*/, ivec2 pos, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
	// 4-----------------7
	// |                 |
	// |   0---------3   |
	// |   |         |   |
	// |   |         |   |
	// |   1-------- 2   |
	// |                 |
	// 5-----------------6
	int bx = (size[0] > 0) ? borderSize : -borderSize;
	int by = (size[1] > 0) ? borderSize : -borderSize;
	ivec2 positions[8] = {
		pos + ivec2(        + bx,         + by), // 0
		pos + ivec2(        + bx, size[1] - by), // 1
		pos + ivec2(size[0] - bx, size[1] - by), // 2
		pos + ivec2(size[0] - bx,         + by), // 3
		pos + ivec2(0           , 0           ), // 4
		pos + ivec2(0           , size[1]     ), // 5
		pos + ivec2(size[0]     , size[1]     ), // 6
		pos + ivec2(size[0]     , 0           ), // 7
	};

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0].get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STREAM_DRAW);

	if (texture.get()) {
		vec2 tex[4] = {
			vec2(0.0f, 0.0f),
			vec2(0.0f, 1.0f),
			vec2(1.0f, 1.0f),
			vec2(1.0f, 0.0f),
		};

		gl::context->progTex.activate();
		glUniform4f(gl::context->unifTexColor,
		            r / 255.0f, g / 255.0f, b / 255.0f, alpha / 255.0f);
		glUniformMatrix4fv(gl::context->unifTexMvp, 1, GL_FALSE,
		                   &gl::context->pixelMvp[0][0]);
		const ivec2* offset = nullptr;
		glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, offset + 4);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1].get());
		glBufferData(GL_ARRAY_BUFFER, sizeof(tex), tex, GL_STREAM_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(1);
		texture.bind();
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(0);
	} else {
		assert(r == 255);
		assert(g == 255);
		assert(b == 255);
		gl::context->progFill.activate();
		glUniformMatrix4fv(gl::context->unifFillMvp, 1, GL_FALSE,
		                   &gl::context->pixelMvp[0][0]);
		glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(0);
		glVertexAttrib4f(1, borderR / 255.0f, borderG / 255.0f, borderB / 255.0f,
		                (borderA * alpha) / (255.0f * 255.0f));

		if ((2 * borderSize >= abs(size[0])) ||
		    (2 * borderSize >= abs(size[1]))) {
			// only border
			glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
		} else {
			// border
			if (borderSize > 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementsBuffer.get());
				glDrawElements(GL_TRIANGLE_STRIP, 10, GL_UNSIGNED_BYTE, nullptr);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}

			// interior
			uint8_t col[4][4] = {
				{ bgR[0], bgG[0], bgB[0], uint8_t((bgA[0] * alpha) / 256) },
				{ bgR[2], bgG[2], bgB[2], uint8_t((bgA[2] * alpha) / 256) },
				{ bgR[3], bgG[3], bgB[3], uint8_t((bgA[3] * alpha) / 256) },
				{ bgR[1], bgG[1], bgB[1], uint8_t((bgA[1] * alpha) / 256) },
			};
			glBindBuffer(GL_ARRAY_BUFFER, vbo[2].get());
			glBufferData(GL_ARRAY_BUFFER, sizeof(col), col, GL_STREAM_DRAW);
			glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr);
			glEnableVertexAttribArray(1);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
			glDisableVertexAttribArray(1);
		}
		glDisableVertexAttribArray(0);
	}
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisable(GL_BLEND);
}

} // namespace openmsx
