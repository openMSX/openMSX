// $Id$

#include "LDSDLRasterizer.hh"
#include "RawFrame.hh"
#include "PostProcessor.hh"
#include "VisibleSurface.hh"
#include "build-info.hh"

namespace openmsx {

template <class Pixel>
LDSDLRasterizer<Pixel>::LDSDLRasterizer(
		VisibleSurface& screen,
		std::auto_ptr<PostProcessor> postProcessor_)
	: postProcessor(postProcessor_)
	, workFrame(new RawFrame(screen.getSDLFormat(), 640, 480))
	, pixelFormat(screen.getSDLFormat())
{
}

template <class Pixel>
LDSDLRasterizer<Pixel>::~LDSDLRasterizer()
{
	delete workFrame;
}

template <class Pixel>
void LDSDLRasterizer<Pixel>::frameStart(EmuTime::param time)
{
	workFrame = postProcessor->rotateFrames(workFrame,
		FrameSource::FIELD_NONINTERLACED, time);
}

template<class Pixel>
void LDSDLRasterizer<Pixel>::drawBlank(int r, int g, int b)
{
	// We should really be presenting the "LASERVISION" text
	// here, like the real laserdisc player does. Note that this
	// changes when seeking or starting to play.
	Pixel background = static_cast<Pixel>(SDL_MapRGB(&pixelFormat, r, g, b));
	for (int y = 0; y < 480; ++y) {
		workFrame->setBlank(y, background);
	}
}

template<class Pixel>
RawFrame* LDSDLRasterizer<Pixel>::getRawFrame()
{
	return workFrame;
}


// Force template instantiation.
#if HAVE_16BPP
template class LDSDLRasterizer<word>;
#endif
#if HAVE_32BPP
template class LDSDLRasterizer<unsigned>;
#endif

} // namespace openmsx
