// $Id$

#include "GLSnow.hh"
#include <cstdlib>


namespace openmsx {

// Dimensions of screen.
static const int WIDTH = 640;
static const int HEIGHT = 480;

GLSnow::GLSnow()
{
	// Create noise texture.
	byte buf[128 * 128];
	for (int i = 0; i < 128 * 128; ++i) {
		buf[i] = (byte)rand();
	}
	glGenTextures(1, &noiseTextureId);
	glBindTexture(GL_TEXTURE_2D, noiseTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 128, 128, 0,
	             GL_LUMINANCE, GL_UNSIGNED_BYTE, buf);

	// Register as display layer.
	Display::INSTANCE->addLayer(this, Display::Z_BACKGROUND);
	Display::INSTANCE->setCoverage(this, Display::COVER_FULL);
}

GLSnow::~GLSnow()
{
	// Free texture.
	glDeleteTextures(1, &noiseTextureId);
}

void GLSnow::paint()
{
	// Draw noise texture.
	double x = (double)rand() / RAND_MAX;
	double y = (double)rand() / RAND_MAX;
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, noiseTextureId);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f + x, 1.5f + y); glVertex2i(   0, HEIGHT - 512);
	glTexCoord2f(3.0f + x, 1.5f + y); glVertex2i(1024, HEIGHT - 512);
	glTexCoord2f(3.0f + x, 0.0f + y); glVertex2i(1024, HEIGHT);
	glTexCoord2f(0.0f + x, 0.0f + y); glVertex2i(   0, HEIGHT);
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

