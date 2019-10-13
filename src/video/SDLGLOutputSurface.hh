#ifndef SDLGLOUTPUTSURFACE_HH
#define SDLGLOUTPUTSURFACE_HH

#include "GLUtil.hh"
#include "MemBuffer.hh"
#include <string>

namespace openmsx {

class OutputSurface;

/** This is a common base class for SDLGLVisibleSurface and
  * SDLGLOffScreenSurface. Its only purpose is to have a place to put common
  * code.
  */
class SDLGLOutputSurface
{
public:
	/** These correspond respectively with the renderers:
	  *   SDLGL-PP, SDLGL-FB16, SDLGL-FB32
	  */
	enum FrameBuffer { FB_NONE, FB_16BPP, FB_32BPP };

	FrameBuffer getFrameBufferType() const { return frameBuffer; }

protected:
	explicit SDLGLOutputSurface(FrameBuffer frameBuffer = FB_NONE);
	~SDLGLOutputSurface() = default;

	void init(OutputSurface& output);
	void flushFrameBuffer(unsigned width, unsigned height);
	void clearScreen();
	void saveScreenshot(const std::string& filename,
	                    const OutputSurface& output) const;

private:
	float texCoordX, texCoordY;
	gl::Texture fbTex;
	MemBuffer<char> fbBuf;
	const FrameBuffer frameBuffer;
};

} // namespace openmsx

#endif
