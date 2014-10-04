#include "GLSaIScaler.hh"

namespace openmsx {

GLSaIScaler::GLSaIScaler()
	: GLScaler("sai")
{
}

void GLSaIScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	setup(superImpose != nullptr);
	execute(src, superImpose,
	        srcStartY, srcEndY, srcWidth,
	        dstStartY, dstEndY, dstWidth,
	        logSrcHeight);
}

} // namespace openmsx
