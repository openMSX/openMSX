// $Id$

#include "GLUtil.hh"
#ifdef __OPENGL_AVAILABLE__


namespace openmsx {

LineTexture::LineTexture() {
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

LineTexture::~LineTexture() {
	glDeleteTextures(1, &textureId);
}

void LineTexture::update(const GLuint *data, int lineWidth) {
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexImage2D(
		GL_TEXTURE_2D,
		0, // level
		GL_RGBA,
		lineWidth, // width
		1, // height
		0, // border
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		data
		);
}

void LineTexture::draw(
	int texX, int screenX, int screenY, int width, int height
) {
	float texL = texX / 512.0f;
	float texR = (texX + width) / 512.0f;
	int screenL = screenX;
	int screenR = screenL + width;
	glBindTexture(GL_TEXTURE_2D, textureId);
	glBegin(GL_QUADS);
	glTexCoord2f(texL, 1); glVertex2i(screenL, screenY);
	glTexCoord2f(texR, 1); glVertex2i(screenR, screenY);
	glTexCoord2f(texR, 0); glVertex2i(screenR, screenY + height);
	glTexCoord2f(texL, 0); glVertex2i(screenL, screenY + height);
	glEnd();
}

} // namespace openmsx

#endif // __OPENGL_AVAILABLE__
