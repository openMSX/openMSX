// $Id$

#include "GLUtil.hh"
#include <cassert>


namespace openmsx {


// Texture
// =======

Texture::Texture() {
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

Texture::~Texture() {
	glDeleteTextures(1, &textureId);
}


// LineTexture
// ===========

LineTexture::LineTexture()
	: Texture()
{
}

void LineTexture::update(const GLuint* data, int lineWidth) {
	bind();
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
	bind();
	glBegin(GL_QUADS);
	glTexCoord2f(texL, 1); glVertex2i(screenL, screenY);
	glTexCoord2f(texR, 1); glVertex2i(screenR, screenY);
	glTexCoord2f(texR, 0); glVertex2i(screenR, screenY + height);
	glTexCoord2f(texL, 0); glVertex2i(screenL, screenY + height);
	glEnd();
}


// StoredFrame
// ===========

// TODO: Get rid of this.
static const int HEIGHT = 480;

StoredFrame::StoredFrame()
	: stored(false)
{
}

void StoredFrame::store() {
	texture.bind();
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1024, 512, 0);
	stored = true;
}

void StoredFrame::draw(int offsetX, int offsetY) {
	assert(stored);
	glEnable(GL_TEXTURE_2D);
	texture.bind();
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	int left = offsetX;
	int right = offsetX + 1024;
	int top = HEIGHT - 512 + offsetY;
	int bottom = HEIGHT + offsetY;
	// TODO: Create display list(s)?
	glBegin(GL_QUADS);
	glTexCoord2i(0, 1); glVertex2i(left, top);
	glTexCoord2i(1, 1); glVertex2i(right, top);
	glTexCoord2i(1, 0); glVertex2i(right, bottom);
	glTexCoord2i(0, 0); glVertex2i(left, bottom);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void StoredFrame::drawBlend(int offsetX, int offsetY, float alpha) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// RGB come from texture, alpha comes from fragment colour.
	glColor4f(1.0, 0.0, 0.0, alpha);
	draw(offsetX, offsetY);
	glDisable(GL_BLEND);
}


} // namespace openmsx

