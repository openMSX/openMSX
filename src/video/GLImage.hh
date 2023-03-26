#ifndef GLTEXTURE_HH
#define GLTEXTURE_HH

#include "GLUtil.hh"
#include "gl_vec.hh"
#include <array>
#include <cstdint>
#include <span>
#include <string>

class SDLSurfacePtr;

namespace openmsx {

class GLImage
{
public:
	explicit GLImage(const std::string& filename);
	explicit GLImage(SDLSurfacePtr image);
	GLImage(const std::string& filename, float scaleFactor);
	GLImage(const std::string& filename, gl::ivec2 size);
	GLImage(gl::ivec2 size, uint32_t rgba);
	GLImage(gl::ivec2 size, std::span<const uint32_t, 4> rgba,
	        int borderSize, uint32_t borderRGBA);

	void draw(gl::ivec2 pos, uint8_t alpha = 255) {
		draw(pos, 255, 255, 255, alpha);
	}
	void draw(gl::ivec2 pos,
	          uint8_t r, uint8_t g, uint8_t b, uint8_t alpha);

	[[nodiscard]] gl::ivec2 getSize() const { return size; }

	/**
	 * Performs a sanity check on image size.
	 * Throws MSXException if width or height is excessively large.
	 * Negative image sizes are valid and flip the image.
	 */
	static void checkSize(gl::ivec2 size);

private:
	void initBuffers();

private:
	gl::ivec2 size;
	std::array<gl::BufferObject, 3> vbo;
	gl::BufferObject elementsBuffer;
	gl::Texture texture{gl::Null()}; // must come after size
	int borderSize{0};
	std::array<uint16_t, 4> bgA; // 0..256
	uint16_t borderA{0}; // 0..256
	std::array<uint8_t, 4> bgR, bgG, bgB;
	uint8_t borderR{0}, borderG{0}, borderB{0};
};

} // namespace openmsx

#endif
