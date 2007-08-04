// $Id$

#include "GLSnow.hh"
#include "openmsx.hh"
#include <cstdlib>

using std::string;

namespace openmsx {

GLSnow::GLSnow(unsigned width_, unsigned height_)
	: Layer(COVER_FULL, Z_BACKGROUND)
	, width(width_), height(height_)
{
	// Create noise texture.
	byte buf[128 * 128];
	for (int i = 0; i < 128 * 128; ++i) {
		buf[i] = byte(rand());
	}
	glGenTextures(1, &noiseTextureId);
	glBindTexture(GL_TEXTURE_2D, noiseTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 128, 128, 0,
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, buf);
}

GLSnow::~GLSnow()
{
	// Free texture.
	glDeleteTextures(1, &noiseTextureId);
}

void GLSnow::paint()
{
	// Rotate and mirror noise texture in consecutive frames to avoid
	// seeing 'patterns' in the noise.
	static const int coord[8][4][2] = {
		{ { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } },
		{ { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } },
		{ { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 } },
		{ { 1, 1 }, { 1, 0 }, { 0, 0 }, { 0, 1 } },
		{ { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 } },
		{ { 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 } },
		{ { 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 } },
		{ { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 } }
	};
	static unsigned cnt = 0;

	// Draw noise texture.
	double x = double(rand()) / RAND_MAX;
	double y = double(rand()) / RAND_MAX;
	cnt = (cnt + 1) % 8;
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, noiseTextureId);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + x, 2.0f + y);
	glVertex2i(coord[cnt][0][0] * width, coord[cnt][0][1] * height);
	glTexCoord2f(2.0f + x, 2.0f + y);
	glVertex2i(coord[cnt][1][0] * width, coord[cnt][1][1] * height);
	glTexCoord2f(2.0f + x, 0.0f + y);
	glVertex2i(coord[cnt][2][0] * width, coord[cnt][2][1] * height);
	glTexCoord2f(0.0f + x, 0.0f + y);
	glVertex2i(coord[cnt][3][0] * width, coord[cnt][3][1] * height);
	glEnd();
	glDisable(GL_TEXTURE_2D);

	// TODO: Mark dirty in 100ms.
}

const string& GLSnow::getName()
{
	static const string NAME = "GLSnow";
	return NAME;
}

} // namespace openmsx

