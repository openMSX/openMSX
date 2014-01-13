#include "GLSaIScaler.hh"

using std::string;

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
		scalerProgram[i].link();

		scalerProgram[i].activate();
		glUniform1i(scalerProgram[i].getUniformLocation("tex"), 0);
		if (i == 1) {
			glUniform1i(scalerProgram[i].getUniformLocation("videoTex"), 1);
		}
		texSizeLoc[i] = scalerProgram[i].getUniformLocation("texSize");
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
