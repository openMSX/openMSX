// $Id$

#include "SDLGLOutputSurface.hh"
#include "OutputSurface.hh"
#include "PNG.hh"
#include "build-info.hh"
#include "Math.hh"
#include "MemBuffer.hh"
#include "vla.hh"
#include <SDL.h>

namespace openmsx {

SDLGLOutputSurface::SDLGLOutputSurface(FrameBuffer frameBuffer_)
	: frameBuffer(frameBuffer_)
{
}

SDLGLOutputSurface::~SDLGLOutputSurface()
{
}

void SDLGLOutputSurface::init(OutputSurface& output)
{
	// This is logically a part of the constructor, but the constructor
	// of the child class needs to run before this code (to create a
	// openGL context). So we split the constructor in two parts, the
	// child class is responsible for calling this second part.

	SDL_PixelFormat format;
	format.palette = nullptr;
	format.colorkey = 0;
	format.alpha = 0;
	if (frameBuffer == FB_16BPP) {
		format.BitsPerPixel = 16;
		format.BytesPerPixel = 2;
		format.Rloss = 3;
		format.Gloss = 2;
		format.Bloss = 3;
		format.Aloss = 8;
		format.Rshift = 11;
		format.Gshift = 5;
		format.Bshift = 0;
		format.Ashift = 0;
		format.Rmask = 0xF800;
		format.Gmask = 0x07E0;
		format.Bmask = 0x001F;
		format.Amask = 0x0000;
	} else {
		// BGRA format
		format.BitsPerPixel = 32;
		format.BytesPerPixel = 4;
		format.Rloss = 0;
		format.Gloss = 0;
		format.Bloss = 0;
		format.Aloss = 0;
		if (OPENMSX_BIGENDIAN) {
			format.Rshift =  8;
			format.Gshift = 16;
			format.Bshift = 24;
			format.Ashift =  0;
			format.Rmask = 0x0000FF00;
			format.Gmask = 0x00FF0000;
			format.Bmask = 0xFF000000;
			format.Amask = 0x000000FF;
		} else {
			format.Rshift = 16;
			format.Gshift =  8;
			format.Bshift =  0;
			format.Ashift = 24;
			format.Rmask = 0x00FF0000;
			format.Gmask = 0x0000FF00;
			format.Bmask = 0x000000FF;
			format.Amask = 0xFF000000;
		}
	}
	output.setSDLFormat(format);

	if (frameBuffer == FB_NONE) {
		output.setBufferPtr(nullptr, 0); // direct access not allowed
	} else {
		// TODO 64 byte aligned (see RawFrame)
		unsigned width  = output.getWidth();
		unsigned height = output.getHeight();
		unsigned texW = Math::powerOfTwo(width);
		unsigned texH = Math::powerOfTwo(height);
		fbBuf.resize(format.BytesPerPixel * texW * texH);
		unsigned pitch = width * format.BytesPerPixel;
		output.setBufferPtr(fbBuf.data(), pitch);

		texCoordX = double(width)  / texW;
		texCoordY = double(height) / texH;

		fbTex.reset(new Texture());
		fbTex->bind();
		if (frameBuffer == FB_16BPP) {
			// TODO: Why use RGB texture instead of RGBA?
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texW, texH, 0,
			             GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fbBuf.data());
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texW, texH, 0,
			             GL_BGRA, GL_UNSIGNED_BYTE, fbBuf.data());
		}
	}
}

void SDLGLOutputSurface::flushFrameBuffer(unsigned width, unsigned height)
{
	assert((frameBuffer == FB_16BPP) || (frameBuffer == FB_32BPP));

	fbTex->bind();
	if (frameBuffer == FB_16BPP) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
		                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fbBuf.data());
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
		                GL_BGRA, GL_UNSIGNED_BYTE, fbBuf.data());
	}

	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0,                GLfloat(texCoordY)); glVertex2i(0,     height);
	glTexCoord2f(GLfloat(texCoordX), GLfloat(texCoordY)); glVertex2i(width, height);
	glTexCoord2f(GLfloat(texCoordX), 0.0               ); glVertex2i(width, 0     );
	glTexCoord2f(0.0,                0.0               ); glVertex2i(0,     0     );
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void SDLGLOutputSurface::saveScreenshot(
	const std::string& filename, unsigned width, unsigned height)
{
	VLA(const void*, rowPointers, height);
	MemBuffer<byte> buffer(width * height * 3);
	for (unsigned i = 0; i < height; ++i) {
		rowPointers[height - 1 - i] = &buffer[width * 3 * i];
	}
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer.data());
	PNG::save(width, height, rowPointers, filename);
}

} // namespace openmsx
