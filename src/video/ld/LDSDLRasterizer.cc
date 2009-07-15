// $Id$

#include "LDSDLRasterizer.hh"
#include "RawFrame.hh"
#include "Display.hh"
#include "Renderer.hh"
#include "RenderSettings.hh"
#include "PostProcessor.hh"
#include "FloatSetting.hh"
#include "StringSetting.hh"
#include "MemoryOps.hh"
#include "VisibleSurface.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

template <class Pixel>
LDSDLRasterizer<Pixel>::LDSDLRasterizer(LaserdiscPlayer& laserdiscPlayer_, 
		Display& display, VisibleSurface& screen_,
		std::auto_ptr<PostProcessor> postProcessor_)
	: laserdiscPlayer(laserdiscPlayer_)
	, screen(screen_)
	, postProcessor(postProcessor_)
	, renderSettings(display.getRenderSettings())
{
	workFrame = new RawFrame(screen.getSDLFormat(), 640, 480);

	// This is the background colour of the Poneer LD-92000
	pixelFormat = screen_.getSDLFormat();
#if 0
	renderSettings.getGamma()      .attach(*this);
	renderSettings.getBrightness() .attach(*this);
	renderSettings.getContrast()   .attach(*this);
	renderSettings.getColorMatrix().attach(*this);
#endif
}

template <class Pixel>
LDSDLRasterizer<Pixel>::~LDSDLRasterizer()
{
#if 0
	renderSettings.getColorMatrix().detach(*this);
	renderSettings.getGamma()      .detach(*this);
	renderSettings.getBrightness() .detach(*this);
	renderSettings.getContrast()   .detach(*this);
#endif

	delete workFrame;
}

template <class Pixel>
bool LDSDLRasterizer<Pixel>::isActive()
{
	return postProcessor->getZ() != Layer::Z_MSX_PASSIVE;
}

template <class Pixel>
void LDSDLRasterizer<Pixel>::reset()
{
	// Init renderer state.
}

template <class Pixel>
void LDSDLRasterizer<Pixel>::frameStart(EmuTime::param time)
{
	workFrame = postProcessor->rotateFrames(workFrame,
		FrameSource::FIELD_NONINTERLACED, time);
}

template <class Pixel>
void LDSDLRasterizer<Pixel>::frameEnd()
{
	// Nothing to do.
}

template<class Pixel>
void LDSDLRasterizer<Pixel>::drawBlank(int r, int g, int b)
{
	// We should really be presenting the "LASERVISION" text
	// here, like the real laserdisc player does. Note that this
	// changes when seeking or starting to play.
	Pixel background = static_cast<Pixel>(SDL_MapRGB(&pixelFormat, r, g, 
									b));

	for (int y=0; y<480; y++) {
		workFrame->setBlank(y, background);
	}
}

template<class Pixel>
void LDSDLRasterizer<Pixel>::drawBitmap(byte *bitmap)
{
	Pixel* line;
	byte r, g, b;

	for (unsigned y=0; y<480; y++) {
		line = workFrame->getLinePtr<Pixel>(y);

		for (unsigned x=0; x<640; x++) {
			r = *bitmap++;
			g = *bitmap++;
			b = *bitmap++;

			*line++ = static_cast<Pixel>(SDL_MapRGB(&pixelFormat, 
								r, g, b));
		}

		workFrame->setLineWidth(y, 640);
	}
}


template<class Pixel>
bool LDSDLRasterizer<Pixel>::isRecording() const
{
	return postProcessor->isRecording();
}


// Force template instantiation.
#if HAVE_16BPP
template class LDSDLRasterizer<word>;
#endif
#if HAVE_32BPP
template class LDSDLRasterizer<unsigned>;
#endif

} // namespace openmsx
