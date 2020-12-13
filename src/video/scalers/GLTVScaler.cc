#include "GLTVScaler.hh"
#include "RenderSettings.hh"

namespace openmsx {

GLTVScaler::GLTVScaler(RenderSettings& renderSettings_)
	: GLScaler("tv")
	, renderSettings(renderSettings_)
{
	for (auto i : xrange(2)) {
		program[i].activate();
		unifMinScanline[i] =
			program[i].getUniformLocation("minScanline");
		unifSizeVariance[i] =
			program[i].getUniformLocation("sizeVariance");
	}
}

void GLTVScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	setup(superImpose != nullptr);
	int i = superImpose ? 1 : 0;
	// These are experimentally established functions that look good.
	// By design, both are 0 for scanline 0.
	float gap = renderSettings.getScanlineGap();
	glUniform1f(unifMinScanline [i], 0.1f * gap + 0.2f * gap * gap);
	glUniform1f(unifSizeVariance[i], 0.7f * gap - 0.3f * gap * gap);
	execute(src, superImpose,
	        srcStartY, srcEndY, srcWidth,
	        dstStartY, dstEndY, dstWidth,
	        logSrcHeight, true);
}

} // namespace openmsx
