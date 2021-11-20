#include "GLSimpleScaler.hh"
#include "RenderSettings.hh"
#include "xrange.hh"

namespace openmsx {

GLSimpleScaler::GLSimpleScaler(
		RenderSettings& renderSettings_, GLScaler& fallback_)
	: GLScaler("simple")
	, renderSettings(renderSettings_)
	, fallback(fallback_)
{
	for (auto i : xrange(2)) {
		program[i].activate();
		unifTexStepX[i] = program[i].getUniformLocation("texStepX");
		unifCnst[i]     = program[i].getUniformLocation("cnst");
	}
}

void GLSimpleScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
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
		setup(superImpose != nullptr);
		if ((blur != 0.0f) && (srcWidth != 1)) { // srcWidth check: workaround for ATI cards
			src.setInterpolation(true);
		}
		GLfloat scan_a = (yScale & 1) ? 0.5f : ((yScale + 1) / (2.0f * yScale));
		GLfloat scan_b = 2.0f - 2.0f * scanline;
		GLfloat scan_c = scanline;
		if (!superImpose) {
			// small optimization in simple.frag:
			// divide by 2 here instead of on every pixel
			scan_b *= 0.5f;
			scan_c *= 0.5f;
		}
		glUniform3f(unifTexStepX[i], 1.0f / srcWidth, 1.0f / srcWidth, 0.0f);
		glUniform4f(unifCnst[i], scan_a, scan_b, scan_c, blur);

		execute(src, superImpose,
		        srcStartY, srcEndY, srcWidth,
		        dstStartY, dstEndY, dstWidth,
		        logSrcHeight);

		src.setInterpolation(false);
	} else {
		fallback.scaleImage(src, superImpose, srcStartY, srcEndY, srcWidth,
		                    dstStartY, dstEndY, dstWidth, logSrcHeight);
	}
}

} // namespace openmsx
