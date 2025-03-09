#include "GLDefaultScaler.hh"

namespace openmsx {

GLDefaultScaler::GLDefaultScaler()
	: GLScaler("default")
{
}

void GLDefaultScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize)
{
	setup(superImpose != nullptr);
	execute(src, superImpose, srcStartY, srcEndY, srcSize, dstSize);
}

} // namespace openmsx
