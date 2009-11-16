// $Id$

#include "GLScaler.hh"
#include "GLUtil.hh"

namespace openmsx {

GLScaler::~GLScaler()
{
}

void GLScaler::uploadBlock(
	unsigned /*srcStartY*/, unsigned /*srcEndY*/,
	unsigned /*lineWidth*/, FrameSource& /*paintFrame*/)
{
}

void GLScaler::drawMultiTex(
	ColorTexture& src,
	unsigned srcStartY, unsigned srcEndY,
	float physSrcHeight, float logSrcHeight,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	src.bind();
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	{
		GLfloat tex0StartY = srcStartY / physSrcHeight;
		GLfloat tex0EndY   = srcEndY   / physSrcHeight;
		GLfloat tex1StartY = srcStartY / logSrcHeight;
		GLfloat tex1EndY   = srcEndY   / logSrcHeight;

		glMultiTexCoord2f(GL_TEXTURE0, 0.0f, tex0StartY);
		glMultiTexCoord2f(GL_TEXTURE1, 0.0f, tex1StartY);
		glVertex2i(       0, dstStartY);

		glMultiTexCoord2f(GL_TEXTURE0, 1.0f, tex0StartY);
		glMultiTexCoord2f(GL_TEXTURE1, 1.0f, tex1StartY);
		glVertex2i(dstWidth, dstStartY);

		glMultiTexCoord2f(GL_TEXTURE0, 1.0f, tex0EndY  );
		glMultiTexCoord2f(GL_TEXTURE1, 1.0f, tex1EndY  );
		glVertex2i(dstWidth, dstEndY  );

		glMultiTexCoord2f(GL_TEXTURE0, 0.0f, tex0EndY  );
		glMultiTexCoord2f(GL_TEXTURE1, 0.0f, tex1EndY  );
		glVertex2i(       0, dstEndY  );
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

} // namespace openmsx
