// $Id: GLRGBScaler.cc 6100 2007-02-19 18:56:40Z m9710797 $

#include "GLRGBScaler.hh"
#include "GLUtil.hh"
#include "RenderSettings.hh"

namespace openmsx {

GLRGBScaler::GLRGBScaler(RenderSettings& renderSettings_)
	: renderSettings(renderSettings_)
{
	VertexShader   vertexShader  ("rgb.vert");
	FragmentShader fragmentShader("rgb.frag");
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
		cnstsLoc   = scalerProgram->getUniformLocation("cnsts");
	}
#endif
}

void GLRGBScaler::scaleImage(
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
	GLfloat height = src.getHeight();
	if (srcWidth != 1) {
		// workaround for ATI cards
		src.enableInterpolation();
	} else {
		// treat border as 256-pixel wide display area
		srcWidth = 320;
	}
	if (GLEW_VERSION_2_0 && ((blur != 0.0f) || (scanline != 1.0f))) {
		scalerProgram->activate();
		glUniform2f(texSizeLoc, srcWidth, height);
		GLfloat a = (yScale & 1) ? 0.5f : ((yScale + 1) / (2.0f * yScale));
		GLfloat c1 = blur;
		GLfloat c2 = 3.0 - 2.0 * c1;
		glUniform4f(cnstsLoc,
		            a,                             // scan_a
		            (1.0f - scanline) * 2.0f * c2, // scan_b_c2
		            scanline * c2,                 // scan_c_c2
		            (c1 - c2) / c2);               // scan_c1_2_2
	}
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
	src.disableInterpolation();
}

} // namespace openmsx
