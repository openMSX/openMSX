// $Id$

#include "GLScaleNxScaler.hh"

namespace openmsx {

GLScaleNxScaler::GLScaleNxScaler()
{
	VertexShader   vertexShader  ("scale2x.vert");
	FragmentShader fragmentShader("scale2x.frag");
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

void GLScaleNxScaler::scaleImage(
	ColorTexture& src, ColorTexture* /*TODO superImpose*/,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned /*logSrcHeight*/)
{
	GLfloat height = GLfloat(src.getHeight());
	if (srcWidth == 320) {
		scalerProgram->activate();
		if (GLEW_VERSION_2_0) {
			glUniform2f(texSizeLoc, 320.0f, height);
		}
	} else {
		scalerProgram->deactivate();
	}

	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
}

} // namespace openmsx
