#include "SDLGLOutputSurface.hh"
#include "GLContext.hh"
#include "OutputSurface.hh"
#include "PNG.hh"
#include "build-info.hh"
#include "Math.hh"
#include "MemBuffer.hh"
#include "vla.hh"
#include <SDL.h>

using namespace gl;

namespace openmsx {

SDLGLOutputSurface::SDLGLOutputSurface(FrameBuffer frameBuffer_)
	: fbTex(Null())
	, frameBuffer(frameBuffer_)
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

		texCoordX = float(width)  / texW;
		texCoordY = float(height) / texH;

		fbTex.allocate();
		fbTex.setInterpolation(false);
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

	fbTex.bind();
	if (frameBuffer == FB_16BPP) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
		                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fbBuf.data());
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
		                GL_BGRA, GL_UNSIGNED_BYTE, fbBuf.data());
	}

	vec2 pos[4] = {
		vec2(0,     height),
		vec2(width, height),
		vec2(width, 0     ),
		vec2(0,     0     ),
	};
	vec2 tex[4] = {
		vec2(0.0f,      texCoordY),
		vec2(texCoordX, texCoordY),
		vec2(texCoordX, 0.0f     ),
		vec2(0.0f,      0.0f     ),
	};
	gl::context->progTex.activate();
	glUniform4f(gl::context->unifTexColor, 1.0f, 1.0f, 1.0f, 1.0f);
	glUniformMatrix4fv(gl::context->unifTexMvp, 1, GL_FALSE,
	                   &gl::context->pixelMvp[0][0]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tex);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
}

void SDLGLOutputSurface::clearScreen()
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void SDLGLOutputSurface::saveScreenshot(
	const std::string& filename, unsigned width, unsigned height)
{
	VLA(const void*, rowPointers, height);
	MemBuffer<uint8_t> buffer(width * height * 3);
	for (unsigned i = 0; i < height; ++i) {
		rowPointers[height - 1 - i] = &buffer[width * 3 * i];
	}
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer.data());
	PNG::save(width, height, rowPointers, filename);
}

} // namespace openmsx
