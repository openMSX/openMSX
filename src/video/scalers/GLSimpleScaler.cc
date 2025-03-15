#include "GLSimpleScaler.hh"
#include "RenderSettings.hh"
#include "narrow.hh"
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
	unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize)
{
	int i = superImpose ? 1 : 0;

	float blur = narrow<float>(renderSettings.getBlurFactor()) * (1.0f / 256.0f);
	float scanline = narrow<float>(renderSettings.getScanlineFactor()) * (1.0f / 255.0f);
	auto yScaleF = float(dstSize.y) / float(srcSize.y);
	if (yScaleF < 1.0f) {
		// less lines in destination than in source
		// (factor=1 / interlace) --> disable scanlines
		scanline = 1.0f;
	}

	if ((blur != 0.0f) || (scanline != 1.0f) || superImpose) {
		setup(superImpose != nullptr, dstSize);
		if ((blur != 0.0f) && (srcSize.x != 1)) { // srcWidth check: workaround for ATI cards
			src.setInterpolation(true);
		}
		float scan_a = (int(yScaleF) & 1) ? 0.5f : ((yScaleF + 1.0f) / (2.0f * yScaleF));
		float scan_b = 2.0f - 2.0f * scanline;
		float scan_c = scanline;
		if (!superImpose) {
			// small optimization in simple.frag:
			// divide by 2 here instead of on every pixel
			scan_b *= 0.5f;
			scan_c *= 0.5f;
		}
		auto recipSrcWidth = 1.0f / narrow<float>(srcSize.x);
		glUniform3f(unifTexStepX[i], recipSrcWidth, recipSrcWidth, 0.0f);
		glUniform4f(unifCnst[i], scan_a, scan_b, scan_c, blur);

		execute(src, superImpose, srcStartY, srcEndY, srcSize, dstSize);

		src.setInterpolation(false);
	} else {
		fallback.scaleImage(src, superImpose, srcStartY, srcEndY, srcSize, dstSize);
	}
}

} // namespace openmsx
