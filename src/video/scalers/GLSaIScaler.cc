#include "GLSaIScaler.hh"
#include "GLPrograms.hh"

using std::string;
using namespace gl;

namespace openmsx {

GLSaIScaler::GLSaIScaler()
	: GLScaler("sai")
{
}

void GLSaIScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	int i = superImpose ? 1 : 0;
	if (superImpose) {
		glActiveTexture(GL_TEXTURE1);
		superImpose->bind();
		glActiveTexture(GL_TEXTURE0);
	}
	program[i].activate();
	glUniform2f(unifTexSize[i], srcWidth, src.getHeight());
	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth);
}

} // namespace openmsx
