// $Id: GLSaIScaler.cc 5474 2006-06-20 20:02:56Z m9710797 $

#include "GLSaIScaler.hh"

namespace openmsx {

GLSaIScaler::GLSaIScaler()
{
	VertexShader   vertexShader  ("sai.vert");
	FragmentShader fragmentShader("sai.frag");
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

void GLSaIScaler::scaleImage(
	ColorTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	GLfloat height = src.getHeight();
	scalerProgram->activate();
	if (GLEW_VERSION_2_0) {
		glUniform2f(texSizeLoc, srcWidth, height);
	}
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
}

} // namespace openmsx
