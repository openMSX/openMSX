#include "GLDefaultScaler.hh"

#include "xrange.hh"

namespace openmsx {

GLDefaultScaler::GLDefaultScaler()
	: GLScaler("default")
{
	for (auto i : xrange(2)) {
		program[i].activate();
		unifTexelCount   [i] = program[i].getUniformLocation("texelCount");
		unifPixelCount   [i] = program[i].getUniformLocation("pixelCount");
		unifTexelSize    [i] = program[i].getUniformLocation("texelSize");
		unifPixelSize    [i] = program[i].getUniformLocation("pixelSize");
		unifHalfTexelSize[i] = program[i].getUniformLocation("halfTexelSize");
		unifHalfPixelSize[i] = program[i].getUniformLocation("halfPixelSize");
		unifYRatio       [i] = program[i].getUniformLocation("yRatio");
	}
}

void GLDefaultScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize)
{
	setup(superImpose != nullptr, dstSize);
	int i = superImpose ? 1 : 0;

	gl::vec2 texelCount{float(srcSize.x), float(src.getHeight())};
	glUniform2f(unifTexelCount[i], texelCount.x, texelCount.y);
	gl::vec2 pixelCount{float(dstSize.x), float(dstSize.y)};
	glUniform2f(unifPixelCount[i], pixelCount.x, pixelCount.y);
	gl::vec2 texelSize = 1.0f / texelCount;
	glUniform2f(unifTexelSize[i], texelSize.x, texelSize.y);
	gl::vec2 pixelSize = 1.0f / pixelCount;
	glUniform2f(unifPixelSize[i], pixelSize.x, pixelSize.y);
	gl::vec2 halfTexelSize = 0.5f * texelSize;
	glUniform2f(unifHalfTexelSize[i], halfTexelSize.x, halfTexelSize.y);
	gl::vec2 halfPixelSize = 0.5f * pixelSize;
	glUniform2f(unifHalfPixelSize[i], halfPixelSize.x, halfPixelSize.y);
	float yRatio = float(src.getHeight()) / float(srcSize.y);
	glUniform2f(unifYRatio[i], yRatio, 1.0f / yRatio);

	src.setInterpolation(false);
	execute(src, superImpose, srcStartY, srcEndY, srcSize, dstSize);
}

} // namespace openmsx
