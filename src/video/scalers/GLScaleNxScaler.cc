#include "GLScaleNxScaler.hh"
#include "GLPrograms.hh"

using std::string;
using namespace gl;

namespace openmsx {

GLScaleNxScaler::GLScaleNxScaler()
{
	for (int i = 0; i < 2; ++i) {
		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  (header, "scale2x.vert");
		FragmentShader fragmentShader(header, "scale2x.frag");
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
		scalerProgram[i].activate();
		glUniform2f(texSizeLoc[i], 320.0f, src.getHeight());
		drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
		             dstStartY, dstEndY, dstWidth);
	} else {
		GLScaler::scaleImage(src, superImpose,
		                     srcStartY, srcEndY, srcWidth,
		                     dstStartY, dstEndY, dstWidth,
		                     logSrcHeight);
	}
}

} // namespace openmsx
