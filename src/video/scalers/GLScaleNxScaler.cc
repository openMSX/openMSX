#include "GLScaleNxScaler.hh"

namespace openmsx {

GLScaleNxScaler::GLScaleNxScaler(GLScaler& fallback_)
	: GLScaler("scale2x")
	, fallback(fallback_)
{
}

void GLScaleNxScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize)
{
	if ((srcSize.x % 320) == 0) {
		setup(superImpose != nullptr, dstSize);
		execute(src, superImpose, srcStartY, srcEndY, srcSize, dstSize);
	} else {
		fallback.scaleImage(src, superImpose, srcStartY, srcEndY, srcSize, dstSize);
	}
}

} // namespace openmsx
