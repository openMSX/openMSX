#ifndef GLDEFAULTSCALER_HH
#define GLDEFAULTSCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"

#include <array>

namespace openmsx {

class GLDefaultScaler final : public GLScaler
{
public:
	GLDefaultScaler();

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize) override;

private:
	std::array<GLint, 2> unifTexelCount;
	std::array<GLint, 2> unifPixelCount;
	std::array<GLint, 2> unifTexelSize; // = 1.0 / texelCount
	std::array<GLint, 2> unifPixelSize; // = 1.0 / pixelCount
	std::array<GLint, 2> unifHalfTexelSize; // = 0.5 * texelSize
	std::array<GLint, 2> unifHalfPixelSize; // = 0.5 * pixelSize
	std::array<GLint, 2> unifYRatio;
};

} // namespace openmsx

#endif // GLDEFAULTSCALER_HH
