// $Id$

#include "GLScaleNxScaler.hh"
#include "GLUtil.hh"

namespace openmsx {

GLScaleNxScaler::GLScaleNxScaler()
{
	scalerProgram.reset(new ShaderProgram());
	scalerFragmentShader.reset(new FragmentShader("scale2x.frag"));
	scalerProgram->attach(*scalerFragmentShader);
	scalerProgram->link();
#ifdef GL_VERSION_2_0
	if (GLEW_VERSION_2_0) {
		scalerProgram->activate();
		GLint texLoc = scalerProgram->getUniformLocation("tex");
		glUniform1i(texLoc, 0);
	}
#endif
}

void GLScaleNxScaler::scaleImage(
	PartialColourTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth
	)
{
	if (srcWidth == 320) {
		scalerProgram->activate();
	} else {
		scalerProgram->deactivate();
	}

	src.drawRect(
		0, srcStartY, srcWidth, srcEndY - srcStartY,
		0, dstStartY, dstWidth, dstEndY - dstStartY
		);
}

} // namespace openmsx
