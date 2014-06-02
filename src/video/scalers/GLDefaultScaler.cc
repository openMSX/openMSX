#include "GLDefaultScaler.hh"
#include "GLPrograms.hh"

using namespace gl;

namespace openmsx {

GLDefaultScaler::GLDefaultScaler()
{
	VertexShader   vertexShader  ("superImpose.vert");
	FragmentShader fragmentShader("superImpose.frag");
	scalerProgram.attach(vertexShader);
	scalerProgram.attach(fragmentShader);
	scalerProgram.bindAttribLocation(0, "a_position");
	scalerProgram.bindAttribLocation(1, "a_texCoord");
	scalerProgram.link();

	scalerProgram.activate();
	glUniform1i(scalerProgram.getUniformLocation("tex"), 0);
	glUniform1i(scalerProgram.getUniformLocation("videoTex"), 1);
	glUniformMatrix4fv(scalerProgram.getUniformLocation("u_mvpMatrix"),
	                   1, GL_FALSE, &pixelMvp[0][0]);
}

void GLDefaultScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned /*srcWidth*/,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	if (superImpose) {
		glActiveTexture(GL_TEXTURE1);
		superImpose->bind();
		glActiveTexture(GL_TEXTURE0);
		scalerProgram.activate();
	} else {
		progTex.activate();
		glUniform4f(unifTexColor, 1.0f, 1.0f, 1.0f, 1.0f);
		glUniformMatrix4fv(unifTexMvp, 1, GL_FALSE, &pixelMvp[0][0]);
	}

	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth);
}

} // namespace openmsx
