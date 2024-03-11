#include "GLImage.hh"
#include "GLContext.hh"
#include "SDLSurfacePtr.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "PNG.hh"
#include "narrow.hh"
#include "xrange.hh"
#include "endian.hh"
#include <cstdint>
#include <cstdlib>
#include <SDL.h>

using namespace gl;

namespace openmsx {

void GLImage::checkSize(gl::ivec2 size)
{
	static constexpr int MAX_SIZE = 2048;
	auto [w, h] = size;
	if (w < -MAX_SIZE || w > MAX_SIZE) {
		throw MSXException("Image width too large: ", w,
		                   " (max ", MAX_SIZE, ')');
	}
	if (h < -MAX_SIZE || h > MAX_SIZE) {
		throw MSXException("Image height too large: ", h,
		                   " (max ", MAX_SIZE, ')');
	}
}

static gl::Texture loadTexture(
	SDLSurfacePtr surface, ivec2& size)
{
	size = ivec2(surface->w, surface->h);
	// Make a copy to convert to the correct pixel format.
	// TODO instead directly load the image in the correct format.
	SDLSurfacePtr image2(size.x, size.y, 32,
		Endian::BIG ? 0xFF000000 : 0x000000FF,
		Endian::BIG ? 0x00FF0000 : 0x0000FF00,
		Endian::BIG ? 0x0000FF00 : 0x00FF0000,
		Endian::BIG ? 0x000000FF : 0xFF000000);

	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = size.x;
	area.h = size.y;
	SDL_SetSurfaceBlendMode(surface.get(), SDL_BLENDMODE_NONE);
	SDL_BlitSurface(surface.get(), &area, image2.get(), &area);

	gl::Texture texture(true); // enable interpolation
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, image2->pixels);
	return texture;
}

gl::Texture loadTexture(const std::string& filename, ivec2& size)
{
	SDLSurfacePtr surface(PNG::load(filename, false));
	try {
		return loadTexture(std::move(surface), size);
	} catch (MSXException& e) {
		throw MSXException("Error loading image ", filename, ": ",
		                   e.getMessage());
	}
}


GLImage::GLImage(const std::string& filename)
	: texture(loadTexture(filename, size))
{
}

GLImage::GLImage(const std::string& filename, float scaleFactor)
	: texture(loadTexture(filename, size))
{
	size = trunc(vec2(size) * scaleFactor);
	checkSize(size);
}

GLImage::GLImage(const std::string& filename, ivec2 size_)
	: texture(loadTexture(filename, size))
{
	checkSize(size_);
	size = size_;
}

GLImage::GLImage(ivec2 size_, uint32_t rgba)
{
	checkSize(size_);
	size = size_;
	for (auto i : xrange(4)) {
		bgR[i] = narrow_cast<uint8_t>((rgba >> 24) & 0xff);
		bgG[i] = narrow_cast<uint8_t>((rgba >> 16) & 0xff);
		bgB[i] = narrow_cast<uint8_t>((rgba >>  8) & 0xff);
		auto alpha = narrow_cast<uint8_t>((rgba >> 0) & 0xff);
		bgA[i] = (alpha == 255) ? 256 : alpha;
	}
	initBuffers();
}

GLImage::GLImage(ivec2 size_, std::span<const uint32_t, 4> rgba,
                 int borderSize_, uint32_t borderRGBA)
	: borderSize(borderSize_)
	, borderR(narrow_cast<uint8_t>((borderRGBA >> 24) & 0xff))
	, borderG(narrow_cast<uint8_t>((borderRGBA >> 16) & 0xff))
	, borderB(narrow_cast<uint8_t>((borderRGBA >>  8) & 0xff))
{
	checkSize(size_);
	size = size_;
	for (auto i : xrange(4)) {
		bgR[i] = narrow_cast<uint8_t>((rgba[i] >> 24) & 0xff);
		bgG[i] = narrow_cast<uint8_t>((rgba[i] >> 16) & 0xff);
		bgB[i] = narrow_cast<uint8_t>((rgba[i] >>  8) & 0xff);
		auto alpha = narrow_cast<uint8_t>((rgba[i] >> 0) & 0xff);
		bgA[i] = (alpha == 255) ? 256 : alpha;
	}

	auto alpha = narrow_cast<uint8_t>((borderRGBA >> 0) & 0xff);
	borderA = (alpha == 255) ? 256 : alpha;

	initBuffers();
}

GLImage::GLImage(SDLSurfacePtr image)
	: texture(loadTexture(std::move(image), size))
{
}

void GLImage::initBuffers()
{
	// border
	std::array<uint8_t, 10> indices = {4, 0, 5, 1, 6, 2, 7, 3, 4, 0};
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementsBuffer.get());
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GLImage::draw(ivec2 pos, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
	// 4-----------------7
	// |                 |
	// |   0---------3   |
	// |   |         |   |
	// |   |         |   |
	// |   1-------- 2   |
	// |                 |
	// 5-----------------6
	int bx = (size.x > 0) ? borderSize : -borderSize;
	int by = (size.y > 0) ? borderSize : -borderSize;
	std::array<ivec2, 8> positions = {
		pos + ivec2(       + bx,        + by), // 0
		pos + ivec2(       + bx, size.y - by), // 1
		pos + ivec2(size.x - bx, size.y - by), // 2
		pos + ivec2(size.x - bx,        + by), // 3
		pos + ivec2(0          , 0          ), // 4
		pos + ivec2(0          , size.y     ), // 5
		pos + ivec2(size.x     , size.y     ), // 6
		pos + ivec2(size.x     , 0          ), // 7
	};

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0].get());
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions.data(), GL_STREAM_DRAW);

	auto& glContext = *gl::context;
	if (texture.get()) {
		std::array<vec2, 4> tex = {
			vec2(0.0f, 0.0f),
			vec2(0.0f, 1.0f),
			vec2(1.0f, 1.0f),
			vec2(1.0f, 0.0f),
		};

		glContext.progTex.activate();
		glUniform4f(glContext.unifTexColor,
		            narrow<float>(r)     * (1.0f / 255.0f),
			    narrow<float>(g)     * (1.0f / 255.0f),
			    narrow<float>(b)     * (1.0f / 255.0f),
			    narrow<float>(alpha) * (1.0f / 255.0f));
		glUniformMatrix4fv(glContext.unifTexMvp, 1, GL_FALSE,
		                   glContext.pixelMvp.data());
		const ivec2* offset = nullptr;
		glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, offset + 4);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1].get());
		glBufferData(GL_ARRAY_BUFFER, sizeof(tex), tex.data(), GL_STREAM_DRAW);
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
		glContext.progFill.activate();
		glUniformMatrix4fv(glContext.unifFillMvp, 1, GL_FALSE,
		                   glContext.pixelMvp.data());
		glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(0);
		glVertexAttrib4f(1,
		                 narrow<float>(borderR) * (1.0f / 255.0f),
		                 narrow<float>(borderG) * (1.0f / 255.0f),
		                 narrow<float>(borderB) * (1.0f / 255.0f),
		                 narrow<float>(borderA * alpha) * (1.0f / (255.0f * 255.0f)));

		if ((2 * borderSize >= abs(size.x)) ||
		    (2 * borderSize >= abs(size.y))) {
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
			std::array col = {
				std::array<uint8_t, 4>{bgR[0], bgG[0], bgB[0], uint8_t((bgA[0] * alpha) / 256)},
				std::array<uint8_t, 4>{bgR[2], bgG[2], bgB[2], uint8_t((bgA[2] * alpha) / 256)},
				std::array<uint8_t, 4>{bgR[3], bgG[3], bgB[3], uint8_t((bgA[3] * alpha) / 256)},
				std::array<uint8_t, 4>{bgR[1], bgG[1], bgB[1], uint8_t((bgA[1] * alpha) / 256)},
			};
			glBindBuffer(GL_ARRAY_BUFFER, vbo[2].get());
			glBufferData(GL_ARRAY_BUFFER, sizeof(col), col.data(), GL_STREAM_DRAW);
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
