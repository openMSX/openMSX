#include "GLScaler.hh"
#include "GLUtil.hh"

using namespace gl;

namespace openmsx {

GLScaler::GLScaler()
{
	VertexShader   vertexShader  ("superImpose.vert");
	FragmentShader fragmentShader("superImpose.frag");
	scalerProgram.attach(vertexShader);
	scalerProgram.attach(fragmentShader);
	scalerProgram.link();

	scalerProgram.activate();
	glUniform1i(scalerProgram.getUniformLocation("tex"), 0);
	glUniform1i(scalerProgram.getUniformLocation("videoTex"), 1);
}

GLScaler::~GLScaler()
{
}

void GLScaler::uploadBlock(
	unsigned /*srcStartY*/, unsigned /*srcEndY*/,
	unsigned /*lineWidth*/, FrameSource& /*paintFrame*/)
{
}

void GLScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned /*srcWidth*/,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	if (superImpose) {
		glActiveTexture(GL_TEXTURE1);
		superImpose->bind();
		glActiveTexture(GL_TEXTURE0);
		scalerProgram.activate();
	} else {
		scalerProgram.deactivate();
	}

	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth);
}

void GLScaler::drawMultiTex(
	ColorTexture& src,
	unsigned srcStartY, unsigned srcEndY,
	float physSrcHeight, float logSrcHeight,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	bool textureFromZero)
{
	src.bind();
	// Note: hShift is pre-divided by srcWidth, while vShift will be divided
	//       by srcHeight later on.
	// Note: The coordinate is put just past zero, to avoid fract() in the
	//       fragment shader to wrap around on rounding errors.
	float hShift = textureFromZero ? 0.501f / dstWidth : 0.0f;
	float vShift = textureFromZero ? 0.501f * (
		float(srcEndY - srcStartY) / float(dstEndY - dstStartY)
		) : 0.0f;
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	{
		GLfloat tex0StartY = (srcStartY + vShift) / physSrcHeight;
		GLfloat tex0EndY   = (srcEndY   + vShift) / physSrcHeight;
		GLfloat tex1StartY = (srcStartY + vShift) / logSrcHeight;
		GLfloat tex1EndY   = (srcEndY   + vShift) / logSrcHeight;

		glMultiTexCoord2f(GL_TEXTURE0, 0.0f + hShift, tex0StartY);
		glMultiTexCoord2f(GL_TEXTURE1, 0.0f + hShift, tex1StartY);
		glVertex2i(       0, dstStartY);

		glMultiTexCoord2f(GL_TEXTURE0, 1.0f + hShift, tex0StartY);
		glMultiTexCoord2f(GL_TEXTURE1, 1.0f + hShift, tex1StartY);
		glVertex2i(dstWidth, dstStartY);

		glMultiTexCoord2f(GL_TEXTURE0, 1.0f + hShift, tex0EndY  );
		glMultiTexCoord2f(GL_TEXTURE1, 1.0f + hShift, tex1EndY  );
		glVertex2i(dstWidth, dstEndY  );

		glMultiTexCoord2f(GL_TEXTURE0, 0.0f + hShift, tex0EndY  );
		glMultiTexCoord2f(GL_TEXTURE1, 0.0f + hShift, tex1EndY  );
		glVertex2i(       0, dstEndY  );
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

} // namespace openmsx
