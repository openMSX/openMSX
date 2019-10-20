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

void SDLGLOutputSurface::init(OutputSurface& output)
{
	// This is logically a part of the constructor, but the constructor
	// of the child class needs to run before this code (to create a
	// openGL context). So we split the constructor in two parts, the
	// child class is responsible for calling this second part.

	SDLAllocFormatPtr format(SDL_AllocFormat(
		        OPENMSX_BIGENDIAN ? SDL_PIXELFORMAT_RGBA8888 :
		                            SDL_PIXELFORMAT_ARGB8888));
	output.setSDLFormat(*format);
	output.setBufferPtr(nullptr, 0); // direct access not allowed
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
