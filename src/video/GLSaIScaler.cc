// $Id: GLSaIScaler.cc 5474 2006-06-20 20:02:56Z m9710797 $

#include "GLSaIScaler.hh"

using std::string;

namespace openmsx {

GLSaIScaler::GLSaIScaler()
{
	for (int i = 0; i < 2; ++i) {
		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  ("sai.vert");
		FragmentShader fragmentShader(header, "sai.frag");
		scalerProgram[i].reset(new ShaderProgram());
		scalerProgram[i]->attach(vertexShader);
		scalerProgram[i]->attach(fragmentShader);
		scalerProgram[i]->link();
#ifdef GL_VERSION_2_0
		if (GLEW_VERSION_2_0) {
			scalerProgram[i]->activate();
			glUniform1i(scalerProgram[i]->getUniformLocation("tex"), 0);
			if (i == 1) {
				glUniform1i(scalerProgram[i]->getUniformLocation("videoTex"), 1);
			}
			texSizeLoc[i] = scalerProgram[i]->getUniformLocation("texSize");
		}
#endif
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
	scalerProgram[i]->activate();
	if (GLEW_VERSION_2_0) {
		glUniform2f(texSizeLoc[i], srcWidth, src.getHeight());
	}
	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth);
}

} // namespace openmsx
