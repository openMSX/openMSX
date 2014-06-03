#include "GLSimpleScaler.hh"
#include "GLUtil.hh"
#include "GLPrograms.hh"
#include "RenderSettings.hh"

using std::string;
using namespace gl;

namespace openmsx {

GLSimpleScaler::GLSimpleScaler(
		RenderSettings& renderSettings_, GLScaler& fallback_)
	: GLScaler("simple")
	, renderSettings(renderSettings_)
	, fallback(fallback_)
{
	for (int i = 0; i < 2; ++i) {
		program[i].activate();
		unifTexStepX[i] = program[i].getUniformLocation("texStepX");
		unifCnst[i]     = program[i].getUniformLocation("cnst");
	}
}

void GLSimpleScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	int i = superImpose ? 1 : 0;

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
		program[i].activate();
		GLfloat scan_a = (yScale & 1) ? 0.5f : ((yScale + 1) / (2.0f * yScale));
		GLfloat scan_b = 2.0f - 2.0f * scanline;
		GLfloat scan_c = scanline;
		if (!superImpose) {
			// small optimization in simple.frag:
			// divide by 2 here instead of on every pixel
			scan_b *= 0.5f;
			scan_c *= 0.5f;
		}
		glUniform2f(unifTexSize[i], srcWidth, src.getHeight());
		glUniform3f(unifTexStepX[i], 1.0f / srcWidth, 1.0f / srcWidth, 0.0f);
		glUniform4f(unifCnst[i], scan_a, scan_b, scan_c, blur);

		drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
			     dstStartY, dstEndY, dstWidth);

		src.disableInterpolation();
	} else {
		fallback.scaleImage(src, superImpose, srcStartY, srcEndY, srcWidth,
		                    dstStartY, dstEndY, dstWidth, logSrcHeight);
	}
}

} // namespace openmsx
