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
	TextureRectangle& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	scalerProgram->activate();
	unsigned inWidth = (srcWidth != 1) ? srcWidth : 640;
	src.drawRect(0, srcStartY, inWidth,  srcEndY - srcStartY,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
}

} // namespace openmsx
