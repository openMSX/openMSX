#include "GLScaleNxScaler.hh"
#include "GLPrograms.hh"

using std::string;
using namespace gl;

namespace openmsx {

GLScaleNxScaler::GLScaleNxScaler(GLScaler& fallback_)
	: GLScaler("scale2x")
	, fallback(fallback_)
{
}

void GLScaleNxScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	if (srcWidth == 320) {
		int i = superImpose ? 1 : 0;
		if (superImpose) {
			glActiveTexture(GL_TEXTURE1);
			superImpose->bind();
			glActiveTexture(GL_TEXTURE0);
		}
		program[i].activate();
		glUniform2f(unifTexSize[i], 320.0f, src.getHeight());
		drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
		             dstStartY, dstEndY, dstWidth);
	} else {
		fallback.scaleImage(src, superImpose,
		                    srcStartY, srcEndY, srcWidth,
		                    dstStartY, dstEndY, dstWidth,
		                    logSrcHeight);
	}
}

} // namespace openmsx
