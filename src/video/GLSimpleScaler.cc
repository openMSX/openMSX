// $Id$

#include "GLSimpleScaler.hh"
#include "GLUtil.hh"
#include "RenderSettings.hh"

using std::string;

namespace openmsx {

GLSimpleScaler::GLSimpleScaler(RenderSettings& renderSettings_)
	: renderSettings(renderSettings_)
{
	for (int i = 0; i < 2; ++i) {
		Data& d = data[i];

		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  ("simple.vert");
		FragmentShader fragmentShader(header, "simple.frag");
		d.scalerProgram.reset(new ShaderProgram());
		d.scalerProgram->attach(vertexShader);
		d.scalerProgram->attach(fragmentShader);
		d.scalerProgram->link();
#ifdef GL_VERSION_2_0
		if (GLEW_VERSION_2_0) {
			data[i].scalerProgram->activate();
			GLint texLoc = d.scalerProgram->getUniformLocation("tex");
			glUniform1i(texLoc, 0);
			if (i == 1) {
				GLint texLoc2 = d.scalerProgram->getUniformLocation("videoTex");
				glUniform1i(texLoc2, 1);
			}
			data[i].texSizeLoc  = d.scalerProgram->getUniformLocation("texSize");
			data[i].texStepXLoc = d.scalerProgram->getUniformLocation("texStepX");
			data[i].cnstLoc     = d.scalerProgram->getUniformLocation("cnst");
		}
#endif
	}
}

void GLSimpleScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	Data& d = data[superImpose ? 1 : 0];

	GLfloat blur = renderSettings.getBlurFactor() / 256.0f;
	GLfloat scanline = renderSettings.getScanlineFactor() / 255.0f;
	unsigned yScale = (dstEndY - dstStartY) / (srcEndY - srcStartY);
	if (yScale == 0) {
		// less lines in destination than in source
		// (factor=1 / interlace) --> disable scanlines
		scanline = 1.0f;
		yScale = 1;
	}

	if ((blur != 0) && (srcWidth != 1)) { // srcWidth check: workaround for ATI cards
		src.enableInterpolation();
	}
	if (GLEW_VERSION_2_0 && ((blur != 0.0) || (scanline != 1.0) || superImpose)) {
		if (superImpose) {
			glActiveTexture(GL_TEXTURE1);
			superImpose->bind();
			glActiveTexture(GL_TEXTURE0);
		}
		d.scalerProgram->activate();
		GLfloat scan_a = (yScale & 1) ? 0.5f : ((yScale + 1) / (2.0f * yScale));
		GLfloat scan_b = 2.0f - 2.0f * scanline;
		GLfloat scan_c = scanline;
		if (!superImpose) {
			// small optimization in simple.frag:
			// divide by 2 here instead of on every pixel
			scan_b *= 0.5f;
			scan_c *= 0.5f;
		}
		glUniform2f(d.texSizeLoc, srcWidth, src.getHeight());
		glUniform3f(d.texStepXLoc, 1.0f / srcWidth, 1.0f / srcWidth, 0.0f);
		glUniform4f(d.cnstLoc, scan_a, scan_b, scan_c, blur);
	}

	// actually draw texture
	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth);
	src.disableInterpolation();
}

} // namespace openmsx
