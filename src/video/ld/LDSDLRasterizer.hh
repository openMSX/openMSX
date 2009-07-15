// $Id$

#ifndef LDSDLRASTERIZER_HH
#define LDSDLRASTERIZER_HH

#include "LDRasterizer.hh"
#include "Observer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class Display;
class OutputSurface;
class VisibleSurface;
class RawFrame;
class RenderSettings;
class Setting;
class PostProcessor;
class LaserdiscPlayer;

/** Rasterizer using a frame buffer approach: it writes pixels to a single
  * rectangular pixel buffer.
  */
template <class Pixel>
class LDSDLRasterizer : public LDRasterizer, private noncopyable
{
public:
	LDSDLRasterizer(LaserdiscPlayer& laserdiscPlayer_, Display& display, 
		VisibleSurface& screen_,
		std::auto_ptr<PostProcessor> postProcessor);
	virtual ~LDSDLRasterizer();

	// Rasterizer interface:
	virtual bool isActive();
	virtual void reset();
	virtual void frameStart(EmuTime::param time);
	virtual void frameEnd();
	virtual bool isRecording() const;

	virtual void drawBlank(int r, int g, int b);
	virtual void drawBitmap(byte *bitmap);
private:
	/** The Laserdisc of which the video output is being rendered.
	  */
	LaserdiscPlayer& laserdiscPlayer;

	/** The surface which is visible to the user.
	  */
	OutputSurface& screen;

	/** The video post processor which displays the frames produced by this
	  *  rasterizer.
	  */
	const std::auto_ptr<PostProcessor> postProcessor;

	/** The next frame as it is delivered by the VDP, work in progress.
	  */
	RawFrame* workFrame;

	/** The current renderer settings (gamma, brightness, contrast)
	  */
	RenderSettings& renderSettings;

	SDL_PixelFormat pixelFormat;
};

} // namespace openmsx

#endif
