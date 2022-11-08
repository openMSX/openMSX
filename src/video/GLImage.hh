#ifndef GLTEXTURE_HH
#define GLTEXTURE_HH

#include "BaseImage.hh"
#include "GLUtil.hh"
#include <array>
#include <cstdint>
#include <span>
#include <string>

class SDLSurfacePtr;

namespace openmsx {

class GLImage final : public BaseImage
{
public:
	GLImage(OutputSurface& output, const std::string& filename);
	GLImage(OutputSurface& output, SDLSurfacePtr image);
	GLImage(OutputSurface& output, const std::string& filename, float scaleFactor);
	GLImage(OutputSurface& output, const std::string& filename, gl::ivec2 size);
	GLImage(OutputSurface& output, gl::ivec2 size, uint32_t rgba);
	GLImage(OutputSurface& output, gl::ivec2 size, std::span<const uint32_t, 4> rgba,
	        int borderSize, uint32_t borderRGBA);

	void draw(OutputSurface& output, gl::ivec2 pos,
	          uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) override;

private:
	void initBuffers();

private:
	std::array<gl::BufferObject, 3> vbo;
	gl::BufferObject elementsBuffer;
	gl::Texture texture; // must come after size
	int borderSize;
	std::array<uint16_t, 4> bgA; // 0..256
	uint16_t borderA; // 0..256
	std::array<uint8_t, 4> bgR, bgG, bgB;
	uint8_t borderR, borderG, borderB;
};

} // namespace openmsx

#endif
