// $Id$

#include "SDLGLOutputSurface.hh"
#include "ScreenShotSaver.hh"
#include "CommandException.hh"
#include "GLSnow.hh"
#include "GLConsole.hh"
#include "IconLayer.hh"
#include <cstdlib>

#include <iostream>

namespace openmsx {

static unsigned roundUpPow2(unsigned x)
{
	unsigned result = 1;
	while (result < x) result <<= 1;
	return result;
}

SDLGLOutputSurface::SDLGLOutputSurface(
		unsigned width, unsigned height, bool fullscreen)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	int flags = SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF |
	            (fullscreen ? SDL_FULLSCREEN : 0);
	//flags |= SDL_RESIZABLE;
	createSurface(width, height, flags);

	format.palette = 0;
	format.BitsPerPixel = 32;
	format.BytesPerPixel = 4;
	format.Rloss = 0;
	format.Gloss = 0;
	format.Bloss = 0;
	format.Aloss = 0;
	format.Rshift = 0;
	format.Gshift = 8;
	format.Bshift = 16;
	format.Ashift = 24;
	format.Rmask = 0x000000FF;
	format.Gmask = 0x0000FF00;
	format.Bmask = 0x00FF0000;
	format.Amask = 0xFF000000;
	format.colorkey = 0;
	format.alpha = 0;

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);

	// TODO 64 byte alligned (see RawFrame)
	unsigned texW = roundUpPow2(width);
	unsigned texH = roundUpPow2(height);
	pitch = width * format.BytesPerPixel;
	data = (char*)malloc(format.BytesPerPixel * texW * texH);

	texCoordX = (double)width  / texW;
	texCoordY = (double)height / texH;

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, data);
}

SDLGLOutputSurface::~SDLGLOutputSurface()
{
	glDeleteTextures(1, &textureId);
	free(data);
}

bool SDLGLOutputSurface::init()
{
	return true;
}

void SDLGLOutputSurface::drawFrameBuffer()
{
	unsigned width  = getWidth();
	unsigned height = getHeight();
	
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
	                GL_RGBA, GL_UNSIGNED_BYTE, data);
	
	glBegin(GL_QUADS);
	glTexCoord2f(0.0,       texCoordY); glVertex2i(0,     height);
	glTexCoord2f(texCoordX, texCoordY); glVertex2i(width, height);
	glTexCoord2f(texCoordX, 0.0      ); glVertex2i(width, 0     );
	glTexCoord2f(0.0,       0.0      ); glVertex2i(0,     0     );
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void SDLGLOutputSurface::finish()
{
	SDL_GL_SwapBuffers();
}

void SDLGLOutputSurface::takeScreenShot(const std::string& filename)
{
	unsigned width  = getWidth();
	unsigned height = getHeight();
	byte** row_pointers = static_cast<byte**>(
		malloc(height * sizeof(byte*)));
	byte* buffer = static_cast<byte*>(
		malloc(width * height * 3 * sizeof(byte)));
	for (unsigned i = 0; i < height; ++i) {
		row_pointers[height - 1 - i] = &buffer[width * 3 * i];
	}
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	try {
		ScreenShotSaver::save(width, height, row_pointers, filename);
	} catch(...) {
		free(row_pointers);
		free(buffer);
		throw;
	}
	free(row_pointers);
	free(buffer);
}

std::auto_ptr<Layer> SDLGLOutputSurface::createSnowLayer()
{
	return std::auto_ptr<Layer>(new GLSnow());
}

std::auto_ptr<Layer> SDLGLOutputSurface::createConsoleLayer(
		MSXMotherBoard& motherboard)
{
	return std::auto_ptr<Layer>(new GLConsole(motherboard));
}

std::auto_ptr<Layer> SDLGLOutputSurface::createIconLayer(
		CommandController& commandController,
		EventDistributor& eventDistributor,
		Display& display)
{
	return std::auto_ptr<Layer>(new GLIconLayer(
			commandController, eventDistributor,
			display, surface));
}

} // namespace openmsx
