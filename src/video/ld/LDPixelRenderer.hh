// $Id$

#ifndef LDPIXELRENDERER_HH
#define LDPIXELRENDERER_HH

#include "LDRenderer.hh"
#include "Observer.hh"
#include "RenderSettings.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class EventDistributor;
class RealTime;
class Display;
class LDRasterizer;
class LaserdiscPlayer;
class Setting;

/** Generic implementation of a pixel-based Renderer.
  * Uses a Rasterizer to plot actual pixels for a specific video system.
  */
class LDPixelRenderer : public LDRenderer, private Observer<Setting>,
			private noncopyable
{
public:
	LDPixelRenderer(LaserdiscPlayer& ld, Display& display);
	virtual ~LDPixelRenderer();

	// Renderer interface:
	virtual void reset(EmuTime::param time);
	virtual void frameStart(EmuTime::param time);
	virtual void frameEnd(EmuTime::param time);

	virtual void drawBlank(int r, int g, int b);
	virtual void drawBitmap(const byte* frame);

private:
	// Observer<Setting> interface:
	virtual void update(const Setting& setting);

	/** Update renderer state to specified moment in time.
	  * @param time Moment in emulated time to update to.
	  * @param force When screen accuracy is used,
	  *     rendering is only performed if this parameter is true.
	  */
	void sync(EmuTime::param time, bool force = false);

	/** Render lines until specified moment in time.
	  * Unlike sync(), this method does not sync with VDPVRAM.
	  * The VRAM should be to be up to date and remain unchanged
	  * from the current time to the specified time.
	  * @param time Moment in emulated time to render lines until.
	  */
	void renderUntil(EmuTime::param time);

	/** The VDP of which the video output is being rendered.
	  */
	LaserdiscPlayer& laserdiscPlayer;

	EventDistributor& eventDistributor;
	RealTime& realTime;
	RenderSettings& renderSettings;

	const std::auto_ptr<LDRasterizer> rasterizer;

	double finishFrameDuration;
	int frameSkipCounter;

	/** Accuracy setting for current frame.
	  */
	RenderSettings::Accuracy accuracy;

	/** Is display enabled?
	  * Enabled means the current line is in the display area and
	  * forced blanking is off.
	  */
	bool displayEnabled;

	/** Should current frame be draw or can it be skipped.
	  */
	bool renderFrame;
	bool prevRenderFrame;
};

} // namespace openmsx

#endif
