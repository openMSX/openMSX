#include "GLSaIScaler.hh"
#include "GLPrograms.hh"

using std::string;
using namespace gl;

namespace openmsx {

GLSaIScaler::GLSaIScaler()
{
	for (int i = 0; i < 2; ++i) {
		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  (header, "sai.vert");
		FragmentShader fragmentShader(header, "sai.frag");
		scalerProgram[i].attach(vertexShader);
		scalerProgram[i].attach(fragmentShader);
		scalerProgram[i].bindAttribLocation(0, "a_position");
		scalerProgram[i].bindAttribLocation(1, "a_texCoord");
		scalerProgram[i].link();

		scalerProgram[i].activate();
		glUniform1i(scalerProgram[i].getUniformLocation("tex"), 0);
		if (i == 1) {
			glUniform1i(scalerProgram[i].getUniformLocation("videoTex"), 1);
		}
		texSizeLoc[i] = scalerProgram[i].getUniformLocation("texSize");
		glUniformMatrix4fv(scalerProgram[i].getUniformLocation("u_mvpMatrix"),
		                   1, GL_FALSE, &pixelMvp[0][0]);
	}
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
	scalerProgram[i].activate();
	glUniform2f(texSizeLoc[i], srcWidth, src.getHeight());
	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth);
}

} // namespace openmsx
