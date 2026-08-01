#include "GLTVScaler.hh"

#include "RenderSettings.hh"

namespace openmsx {

GLTVScaler::GLTVScaler(RenderSettings& renderSettings_)
	: GLScaler("tv")
	, renderSettings(renderSettings_)
{
	for (auto i : xrange(2)) {
		program[i].activate();
		unifMinScanline[i]  = program[i].getUniformLocation("minScanline");
		unifSizeVariance[i] = program[i].getUniformLocation("sizeVariance");
		unifDstSize[i]      = program[i].getUniformLocation("dstSize");
	}
}

void GLTVScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize)
{
	setup(superImpose != nullptr, dstSize);
	int i = superImpose ? 1 : 0;
	// These are experimentally established functions that look good.
	// By design, both are 0 for scanline 0.
	float gap = renderSettings.getScanlineGap();
	glUniform1f(unifMinScanline [i], 0.1f * gap + 0.2f * gap * gap);
	glUniform1f(unifSizeVariance[i], 0.7f * gap - 0.3f * gap * gap);
	glUniform2f(unifDstSize[i], float(dstSize.x), float(dstSize.y));
	execute(src, superImpose, srcStartY, srcEndY, srcSize, dstSize);
}

} // namespace openmsx
