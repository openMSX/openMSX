#include "GLRGBScaler.hh"
#include "RenderSettings.hh"

namespace openmsx {

GLRGBScaler::GLRGBScaler(
		RenderSettings& renderSettings_, GLScaler& fallback_)
	: GLScaler("rgb")
	, renderSettings(renderSettings_)
	, fallback(fallback_)
{
	for (auto i : xrange(2)) {
		program[i].activate();
		unifCnsts[i] = program[i].getUniformLocation("cnsts");
	}
}

void GLRGBScaler::scaleImage(
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
		if (srcWidth != 1) {
			// workaround for ATI cards
			src.setInterpolation(true);
		} else {
			// treat border as 256-pixel wide display area
			srcWidth = 320;
		}
		GLfloat a = (yScale & 1) ? 0.5f : ((yScale + 1) / (2.0f * yScale));
		GLfloat c1 = blur;
		GLfloat c2 = 3.0f - 2.0f * c1;
		glUniform4f(unifCnsts[i],
		            a,                             // scan_a
		            (1.0f - scanline) * 2.0f * c2, // scan_b_c2
		            scanline * c2,                 // scan_c_c2
		            (c1 - c2) / c2);               // scan_c1_2_2
		execute(src, superImpose,
		        srcStartY, srcEndY, srcWidth,
		        dstStartY, dstEndY, dstWidth,
		        logSrcHeight);
		src.setInterpolation(false);
	} else {
		fallback.scaleImage(src, superImpose,
		                    srcStartY, srcEndY, srcWidth,
		                    dstStartY, dstEndY, dstWidth,
		                    logSrcHeight);
	}
}

} // namespace openmsx
