#include "GLDefaultScaler.hh"

namespace openmsx {

GLDefaultScaler::GLDefaultScaler()
	: GLScaler("default")
{
}

void GLDefaultScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	execute(src, superImpose,
	        srcStartY, srcEndY, srcWidth,
	        dstStartY, dstEndY, dstWidth,
	        logSrcHeight);
}

} // namespace openmsx
