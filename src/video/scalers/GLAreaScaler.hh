#ifndef GLAREASCALER_HH
#define GLAREASCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"

#include <array>

namespace openmsx {

class GLAreaScaler final : public GLScaler
{
public:
	explicit GLAreaScaler();

	void scaleImage(
		gl::ColorTexture& src, gl::ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize) override;

private:
	std::array<GLint, 2> unifTexelCount;
	std::array<GLint, 2> unifPixelCount;
	std::array<GLint, 2> unifTexelSize;
	std::array<GLint, 2> unifPixelSize;
};

} // namespace openmsx

#endif // GLTVSCALER_HH
