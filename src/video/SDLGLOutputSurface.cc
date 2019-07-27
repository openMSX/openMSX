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

	SDLAllocFormatPtr format(SDL_AllocFormat(
		(frameBuffer == FB_16BPP) ? SDL_PIXELFORMAT_RGB24 :
		        OPENMSX_BIGENDIAN ? SDL_PIXELFORMAT_RGBA8888 :
		                            SDL_PIXELFORMAT_ARGB8888));
	output.setSDLFormat(*format);

	if (frameBuffer == FB_NONE) {
		output.setBufferPtr(nullptr, 0); // direct access not allowed
	} else {
		// TODO 64 byte aligned (see RawFrame)
		unsigned width  = output.getWidth();
		unsigned height = output.getHeight();
		unsigned texW = Math::ceil2(width);
		unsigned texH = Math::ceil2(height);
		fbBuf.resize(format->BytesPerPixel * texW * texH);
		unsigned pitch = width * format->BytesPerPixel;
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
	const std::string& filename, const OutputSurface& output) const
{
	gl::ivec2 offset = output.getViewOffset();
	gl::ivec2 size   = output.getViewSize();

	VLA(const void*, rowPointers, size[1]);
	MemBuffer<uint8_t> buffer(size[0] * size[1] * 3);
	for (int i = 0; i < size[1]; ++i) {
		rowPointers[size[1] - 1 - i] = &buffer[size[0] * 3 * i];
	}
	glReadPixels(offset[0], offset[1], size[0], size[1], GL_RGB, GL_UNSIGNED_BYTE, buffer.data());
	PNG::save(size[0], size[1], rowPointers, filename);
}

} // namespace openmsx
