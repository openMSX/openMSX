// $Id$

#include "GLUtil.hh"
#include <cassert>


namespace openmsx {


// class Texture

Texture::Texture()
{
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

Texture::~Texture()
{
	glDeleteTextures(1, &textureId);
}


// class LineTexture

LineTexture::LineTexture()
	: Texture()
{
}

void LineTexture::update(const GLuint* data, int lineWidth)
{
	bind();
	glTexImage2D(GL_TEXTURE_2D,
	             0, // level
	             GL_RGBA,
	             lineWidth, // width
	             1, // height
	             0, // border
	             GL_RGBA,
	             GL_UNSIGNED_BYTE,
	             data);
}

void LineTexture::draw(
	int texX, int screenX, int screenY, int width, int height)
{
	double texL = texX / 512.0f;
	double texR = (texX + width) / 512.0f;
	int screenL = screenX;
	int screenR = screenL + width;
	bind();
	glBegin(GL_QUADS);
	glTexCoord2f(texL, 1.0f); glVertex2i(screenL, screenY);
	glTexCoord2f(texR, 1.0f); glVertex2i(screenR, screenY);
	glTexCoord2f(texR, 0.0f); glVertex2i(screenR, screenY + height);
	glTexCoord2f(texL, 0.0f); glVertex2i(screenL, screenY + height);
	glEnd();
}


// class StoredFrame

static unsigned powerOfTwo(unsigned a)
{
	unsigned res = 1;
	while (a > res) res <<= 1;
	return res;
}

StoredFrame::StoredFrame()
	: stored(false)
{
}

void StoredFrame::store(unsigned x, unsigned y)
{
	texture.bind();
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0,
	                 powerOfTwo(x), powerOfTwo(y), 0);
	stored = true;
	width = x;
	height = y;
}

void StoredFrame::draw(int offsetX, int offsetY)
{
	assert(stored);
	glEnable(GL_TEXTURE_2D);
	texture.bind();
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	// TODO: Create display list(s)?
	float x = static_cast<float>(width)  / powerOfTwo(width);
	float y = static_cast<float>(height) / powerOfTwo(height);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f,    y); glVertex2i(offsetX,       offsetY      );
	glTexCoord2f(   x,    y); glVertex2i(offsetX + 640, offsetY      );
	glTexCoord2f(   x, 0.0f); glVertex2i(offsetX + 640, offsetY + 480);
	glTexCoord2f(0.0f, 0.0f); glVertex2i(offsetX,       offsetY + 480);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void StoredFrame::drawBlend(int offsetX, int offsetY, double alpha)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// RGB come from texture, alpha comes from fragment colour.
	glColor4f(1.0, 0.0, 0.0, alpha);
	draw(offsetX, offsetY);
	glDisable(GL_BLEND);
}

} // namespace openmsx

