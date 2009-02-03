// $Id$

#include "GLSimpleScaler.hh"
#include "GLUtil.hh"
#include "RenderSettings.hh"

namespace openmsx {

GLSimpleScaler::GLSimpleScaler(RenderSettings& renderSettings_)
	: renderSettings(renderSettings_)
{
	VertexShader   vertexShader  ("simple.vert");
	FragmentShader fragmentShader("simple.frag");
	scalerProgram.reset(new ShaderProgram());
	scalerProgram->attach(vertexShader);
	scalerProgram->attach(fragmentShader);
	scalerProgram->link();
#ifdef GL_VERSION_2_0
	if (GLEW_VERSION_2_0) {
		scalerProgram->activate();
		GLint texLoc = scalerProgram->getUniformLocation("tex");
		glUniform1i(texLoc, 0);
		texSizeLoc  = scalerProgram->getUniformLocation("texSize");
		texStepXLoc = scalerProgram->getUniformLocation("texStepX");
		cnstLoc     = scalerProgram->getUniformLocation("cnst");
	}
#endif
}

void GLSimpleScaler::scaleImage(
	ColorTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	GLfloat blur = renderSettings.getBlurFactor() / 256.0f;
	GLfloat scanline = renderSettings.getScanlineFactor() / 255.0f;
	unsigned yScale = (dstEndY - dstStartY) / (srcEndY - srcStartY);
	if (yScale == 0) {
		// less lines in destination than in source
		// (factor=1 / interlace) --> disable scanlines
		scanline = 1.0f;
		yScale = 1;
	}
	GLfloat height = GLfloat(src.getHeight());
	if ((blur != 0) && (srcWidth != 1)) { // srcWidth check: workaround for ATI cards
		src.enableInterpolation();
	}
	if (GLEW_VERSION_2_0 && ((blur != 0.0) || (scanline != 1.0))) {
		scalerProgram->activate();
		GLfloat a = (yScale & 1) ? 0.5f : ((yScale + 1) / (2.0f * yScale));
		glUniform2f(texSizeLoc, GLfloat(srcWidth), height);
		glUniform3f(texStepXLoc, 1.0f / srcWidth, 1.0f / srcWidth, 0.0f);
		glUniform4f(cnstLoc, a,                // scan_a
		                     1.0f - scanline,  // scan_b
		                     scanline * 0.5f,  // scan_c
		                     blur);            // alpha
	}
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
	src.disableInterpolation();
}

} // namespace openmsx
