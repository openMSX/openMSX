// $Id$

#include "GLRGBScaler.hh"
#include "GLUtil.hh"
#include "RenderSettings.hh"
#include "memory.hh"

using std::string;

namespace openmsx {

GLRGBScaler::GLRGBScaler(RenderSettings& renderSettings_)
	: renderSettings(renderSettings_)
{
	for (int i = 0; i < 2; ++i) {
		Data& d = data[i];

		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  (header, "rgb.vert");
		FragmentShader fragmentShader(header, "rgb.frag");
		d.scalerProgram.attach(vertexShader);
		d.scalerProgram.attach(fragmentShader);
		d.scalerProgram.link();
#ifdef GL_VERSION_2_0
		if (GLEW_VERSION_2_0) {
			d.scalerProgram.activate();
			glUniform1i(d.scalerProgram.getUniformLocation("tex"), 0);
			if (i == 1) {
				glUniform1i(d.scalerProgram.getUniformLocation("videoTex"), 1);
			}
			d.texSizeLoc = d.scalerProgram.getUniformLocation("texSize");
			d.cnstsLoc   = d.scalerProgram.getUniformLocation("cnsts");
		}
#endif
	}
}

void GLRGBScaler::scaleImage(
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
	if (srcWidth != 1) {
		// workaround for ATI cards
		src.enableInterpolation();
	} else {
		// treat border as 256-pixel wide display area
		srcWidth = 320;
	}
	if (GLEW_VERSION_2_0 && ((blur != 0.0f) || (scanline != 1.0f) || superImpose)) {
		if (superImpose) {
			glActiveTexture(GL_TEXTURE1);
			superImpose->bind();
			glActiveTexture(GL_TEXTURE0);
		}
		d.scalerProgram.activate();
		glUniform2f(d.texSizeLoc, srcWidth, src.getHeight());
		GLfloat a = (yScale & 1) ? 0.5f : ((yScale + 1) / (2.0f * yScale));
		GLfloat c1 = blur;
		GLfloat c2 = 3.0f - 2.0f * c1;
		glUniform4f(d.cnstsLoc,
		            a,                             // scan_a
		            (1.0f - scanline) * 2.0f * c2, // scan_b_c2
		            scanline * c2,                 // scan_c_c2
		            (c1 - c2) / c2);               // scan_c1_2_2
	}
	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth);
	src.disableInterpolation();
}

} // namespace openmsx
