// $Id$

#include "GLTVScaler.hh"

namespace openmsx {

GLTVScaler::GLTVScaler()
{
	// Initialise shaders.
	VertexShader   vertexShader  ("tv.vert");
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
		texSizeLoc = scalerProgram->getUniformLocation("texSize");
	}
#endif
}

GLTVScaler::~GLTVScaler()
{
}

void GLTVScaler::scaleImage(
	ColourTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned /*srcWidth*/,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	GLfloat height = src.getHeight();
	scalerProgram->activate();
	if (GLEW_VERSION_2_0) {
		// always do as-if there are 640 dots on a line to get the
		// same dot effect in border, 256 and 512 pixel areas
		glUniform2f(texSizeLoc, 640.0f, height);
	}
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
}

} // namespace openmsx
