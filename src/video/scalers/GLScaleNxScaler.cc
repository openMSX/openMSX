#include "GLScaleNxScaler.hh"

namespace openmsx {

GLScaleNxScaler::GLScaleNxScaler(GLScaler& fallback_)
	: GLScaler("scale2x")
	, fallback(fallback_)
{
}

void GLScaleNxScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	if (srcWidth == 320) {
		setup(superImpose != nullptr);
		execute(src, superImpose,
		        srcStartY, srcEndY, srcWidth,
		        dstStartY, dstEndY, dstWidth,
		        logSrcHeight);
	} else {
		fallback.scaleImage(src, superImpose,
		                    srcStartY, srcEndY, srcWidth,
		                    dstStartY, dstEndY, dstWidth,
		                    logSrcHeight);
	}
}

} // namespace openmsx
