#include "GLAreaScaler.hh"

#include "xrange.hh"

namespace openmsx {

GLAreaScaler::GLAreaScaler()
	: GLScaler("area")
{
	for (auto i : xrange(2)) {
		program[i].activate();
		unifTexelCount[i] = program[i].getUniformLocation("texelCount");
		unifPixelCount[i] = program[i].getUniformLocation("pixelCount");
		unifTexelSize [i] = program[i].getUniformLocation("texelSize");
		unifPixelSize [i] = program[i].getUniformLocation("pixelSize");
	}
}

void GLAreaScaler::scaleImage(
	gl::ColorTexture& src, gl::ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, gl::ivec2 srcSize, gl::ivec2 dstSize)
{
	setup(superImpose != nullptr);
	int i = superImpose ? 1 : 0;
	glUniform2f(unifTexelCount[i], float(srcSize.x), float(src.getHeight()));
	glUniform2f(unifPixelCount[i], float(dstSize.x), float(dstSize.y));
	glUniform2f(unifTexelSize[i], 1.0f / float(srcSize.x), 1.0f / float(src.getHeight()));
	glUniform2f(unifPixelSize[i], 1.0f / float(dstSize.x), 1.0f / float(dstSize.y));
	src.setInterpolation(false);
	execute(src, superImpose, srcStartY, srcEndY, srcSize, dstSize);
}

} // namespace openmsx
