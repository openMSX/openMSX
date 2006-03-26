// $Id$

#include "GLTVScaler.hh"
#include "GLUtil.hh"

namespace openmsx {

GLTVScaler::GLTVScaler()
{
	// Initialise shaders.
	VertexShader vertexShader("tv.vert");
	FragmentShader fragmentShader("tv.frag");
	scalerProgram.reset(new ShaderProgram());
	scalerProgram->attach(vertexShader);
	scalerProgram->attach(fragmentShader);
	scalerProgram->link();
#ifdef GL_VERSION_2_0
	if (GLEW_VERSION_2_0) {
		scalerProgram->activate();
		GLint texLoc = scalerProgram->getUniformLocation("tex");
		glUniform1i(texLoc, 0);
	}
#endif
}

GLTVScaler::~GLTVScaler()
{
}

void GLTVScaler::scaleImage(
	PartialColourTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth
	)
{
	scalerProgram->activate();
	src.drawRect(
		0, srcStartY, srcWidth, srcEndY - srcStartY,
		0, dstStartY, dstWidth, dstEndY - dstStartY
		);
}

} // namespace openmsx
