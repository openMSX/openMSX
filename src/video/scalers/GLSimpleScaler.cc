#include "GLSimpleScaler.hh"
#include "GLUtil.hh"
#include "GLPrograms.hh"
#include "RenderSettings.hh"

using std::string;
using namespace gl;

namespace openmsx {

GLSimpleScaler::GLSimpleScaler(
		RenderSettings& renderSettings_, GLScaler& fallback_)
	: renderSettings(renderSettings_)
	, fallback(fallback_)
{
	for (int i = 0; i < 2; ++i) {
		Data& d = data[i];

		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  (header, "simple.vert");
		FragmentShader fragmentShader(header, "simple.frag");
		d.scalerProgram.attach(vertexShader);
		d.scalerProgram.attach(fragmentShader);
		d.scalerProgram.bindAttribLocation(0, "a_position");
		d.scalerProgram.bindAttribLocation(1, "a_texCoord");
		d.scalerProgram.link();

		data[i].scalerProgram.activate();
		glUniform1i(d.scalerProgram.getUniformLocation("tex"), 0);
		if (i == 1) {
			glUniform1i(d.scalerProgram.getUniformLocation("videoTex"), 1);
		}
		data[i].texSizeLoc  = d.scalerProgram.getUniformLocation("texSize");
		data[i].texStepXLoc = d.scalerProgram.getUniformLocation("texStepX");
		data[i].cnstLoc     = d.scalerProgram.getUniformLocation("cnst");
		glUniformMatrix4fv(d.scalerProgram.getUniformLocation("u_mvpMatrix"),
		                   1, GL_FALSE, &pixelMvp[0][0]);
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

	if ((blur != 0.0f) || (scanline != 1.0f) || superImpose) {
		if ((blur != 0.0f) && (srcWidth != 1)) { // srcWidth check: workaround for ATI cards
			src.enableInterpolation();
		}
		if (superImpose) {
			glActiveTexture(GL_TEXTURE1);
			superImpose->bind();
			glActiveTexture(GL_TEXTURE0);
		}
		d.scalerProgram.activate();
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

		drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
			     dstStartY, dstEndY, dstWidth);

		src.disableInterpolation();
	} else {
		fallback.scaleImage(src, superImpose, srcStartY, srcEndY, srcWidth,
		                    dstStartY, dstEndY, dstWidth, logSrcHeight);
	}
}

} // namespace openmsx
