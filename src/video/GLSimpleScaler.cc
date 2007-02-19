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
		texSizeLoc = scalerProgram->getUniformLocation("texSize");
		alphaLoc   = scalerProgram->getUniformLocation("alpha");
		scanALoc   = scalerProgram->getUniformLocation("scan_a");
		scanBLoc   = scalerProgram->getUniformLocation("scan_b");
		scanCLoc   = scalerProgram->getUniformLocation("scan_c");
	}
#endif
}

void GLSimpleScaler::scaleImage(
	ColourTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	GLfloat blur = renderSettings.getBlurFactor() / 256.0;
	GLfloat scanline = renderSettings.getScanlineFactor() / 255.0;
	unsigned yScale = (dstEndY - dstStartY) / (srcEndY - srcStartY);
	if (yScale == 0) {
		// less lines in destination than in source
		// (factor=1 / interlace) --> disable scanlines
		scanline = 1.0f;
		yScale = 1;
	}
	GLfloat height = src.getHeight();
	if ((blur != 0) && (srcWidth != 1)) { // srcWidth check: workaround for ATI cards
		src.enableInterpolation();
	}
	if (GLEW_VERSION_2_0 && ((blur != 0.0) || (scanline != 1.0))) {
		scalerProgram->activate();
		GLfloat a = (yScale & 1) ? 0.5f : ((yScale + 1) / (2.0f * yScale));
		glUniform2f(texSizeLoc, srcWidth, height);
		glUniform1f(alphaLoc, blur);
		glUniform1f(scanALoc, a);
		glUniform1f(scanBLoc, 1.0f - scanline);
		glUniform1f(scanCLoc, scanline * 0.5f);
	}
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
	src.disableInterpolation();
}

} // namespace openmsx
