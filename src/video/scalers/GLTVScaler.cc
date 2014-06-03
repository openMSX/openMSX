#include "GLTVScaler.hh"
#include "GLPrograms.hh"
#include "RenderSettings.hh"

using std::string;
using namespace gl;

namespace openmsx {

GLTVScaler::GLTVScaler(RenderSettings& renderSettings_)
	: GLScaler("tv")
	, renderSettings(renderSettings_)
{
	for (int i = 0; i < 2; ++i) {
		program[i].activate();
		unifMinScanline[i] =
			program[i].getUniformLocation("minScanline");
		unifSizeVariance[i] =
			program[i].getUniformLocation("sizeVariance");
	}
}

void GLTVScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned /*srcWidth*/,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	int i = superImpose ? 1 : 0;
	if (superImpose) {
		glActiveTexture(GL_TEXTURE1);
		superImpose->bind();
		glActiveTexture(GL_TEXTURE0);
	}
	program[i].activate();
	glUniform3f(unifTexSize[i], src.getWidth(), src.getHeight(), logSrcHeight);
	// These are experimentally established functions that look good.
	// By design, both are 0 for scanline 0.
	float gap = renderSettings.getScanlineGap();
	glUniform1f(unifMinScanline [i], 0.1f * gap + 0.2f * gap * gap);
	glUniform1f(unifSizeVariance[i], 0.7f * gap - 0.3f * gap * gap);
	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth, true);
}

} // namespace openmsx
