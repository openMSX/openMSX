// $Id$

#ifndef __PIXELRENDERER_HH__
#define __PIXELRENDERER_HH__

#include "Renderer.hh"
#include "VDPVRAM.hh"
#include "CircularBuffer.hh"
#include "SettingListener.hh"
#include "RenderSettings.hh"
#include "DisplayMode.hh"
#include "openmsx.hh"


namespace openmsx {

class Rasterizer;
class VDP;
class SpriteChecker;


/** Generic implementation of a pixel-based Renderer.
  * Uses a Rasterizer to plot actual pixels for a specific video system.
  */
class PixelRenderer : public Renderer, private SettingListener
{
public:
	/** Constructor.
	  */
	PixelRenderer(
		RendererFactory::RendererID id, VDP* vdp, Rasterizer* rasterizer );

	/** Destructor.
	  */
	virtual ~PixelRenderer();

	// Renderer interface:
	virtual void reset(const EmuTime& time);
	virtual bool checkSettings();
	virtual void frameStart(const EmuTime &time);
	virtual void frameEnd(const EmuTime &time);
	virtual void updateHorizontalScrollLow(byte scroll, const EmuTime &time);
	virtual void updateHorizontalScrollHigh(byte scroll, const EmuTime &time);
	virtual void updateBorderMask(bool masked, const EmuTime &time);
	virtual void updateMultiPage(bool multiPage, const EmuTime &time);
	virtual void updateTransparency(bool enabled, const EmuTime& time);
	virtual void updateForegroundColour(int colour, const EmuTime& time);
	virtual void updateBackgroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkForegroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkBackgroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkState(bool enabled, const EmuTime& time);
	virtual void updatePalette(int index, int grb, const EmuTime& time);
	virtual void updateVerticalScroll(int scroll, const EmuTime& time);
	virtual void updateHorizontalAdjust(int adjust, const EmuTime& time);
	virtual void updateDisplayEnabled(bool enabled, const EmuTime &time);
	virtual void updateDisplayMode(DisplayMode mode, const EmuTime& time);
	virtual void updateNameBase(int addr, const EmuTime& time);
	virtual void updatePatternBase(int addr, const EmuTime& time);
	virtual void updateColourBase(int addr, const EmuTime& time);
	virtual void updateSpritesEnabled(bool enabled, const EmuTime &time);
	virtual void updateVRAM(unsigned offset, const EmuTime &time);
	virtual void updateWindow(bool enabled, const EmuTime &time);
	virtual float getFrameRate() const;

private:
	/** Indicates whether the area to be drawn is border or display. */
	enum DrawType { DRAW_BORDER, DRAW_DISPLAY, DRAW_SPRITES };

	// SettingListener interface:
	virtual void update(const SettingLeafNode* setting);

	/** Call the right draw method in the subclass,
	  * depending on passed drawType.
	  */
	inline void draw(
		int startX, int startY, int endX, int endY, DrawType drawType,
		bool atEnd);

	/** Subdivide an area specified by two scan positions into a series of
	  * rectangles.
	  * Clips the rectangles to { (x,y) | clipL <= x < clipR }.
	  * @param drawType
	  *   If DRAW_BORDER, draw rectangles using drawBorder;
	  *   if DRAW_DISPLAY, draw rectangles using drawDisplay.
	  */
	inline void subdivide(
		int startX, int startY, int endX, int endY,
		int clipL, int clipR, DrawType drawType );

	inline bool checkSync(int offset, const EmuTime &time);

	/** Update renderer state to specified moment in time.
	  * @param time Moment in emulated time to update to.
	  * @param force When screen accuracy is used,
	  * 	rendering is only performed if this parameter is true.
	  */
	void sync(const EmuTime &time, bool force = false) {
		// Synchronisation is done in two phases:
		// 1. update VRAM
		// 2. update other subsystems
		// Note that as part of step 1, type 2 updates can be triggered.
		// Executing step 2 takes care of the subsystem changes that occur
		// after the last VRAM update.
		// This scheme makes sure type 2 routines such as renderUntil and
		// checkUntil are not re-entered, which was causing major pain in
		// the past.
		// TODO: I wonder if it's possible to enforce this synchronisation
		//       scheme at a higher level. Probably. But how...
		//if ((frameSkipCounter == 0) && TODO
		if (accuracy != RenderSettings::ACC_SCREEN || force) {
			vram->sync(time);
			renderUntil(time);
		}
	}

	/** Render lines until specified moment in time.
	  * Unlike sync(), this method does not sync with VDPVRAM.
	  * The VRAM should be to be up to date and remain unchanged
	  * from the current time to the specified time.
	  * @param time Moment in emulated time to render lines until.
	  */
	void renderUntil(const EmuTime &time);

	/** The VDP of which the video output is being rendered.
	  */
	VDP *vdp;

	/** The VRAM whose contents are rendered.
	  */
	VDPVRAM *vram;

	/** The sprite checker whose sprites are rendered.
	  */
	SpriteChecker *spriteChecker;

	/** Accuracy setting for current frame.
	  */
	RenderSettings::Accuracy accuracy;

	/** Is display enabled?
	  * Enabled means the current line is in the display area and
	  * forced blanking is off.
	  */
	bool displayEnabled;

	/** Number of the next position within a line to render.
	  * Expressed in VDP clock ticks since start of line.
	  */
	int nextX;

	/** Number of the next line to render.
	  * Expressed in number of lines since start of frame.
	  */
	int nextY;

	int frameSkipCounter;
	float finishFrameDuration;
	
	static const unsigned NUM_FRAME_DURATIONS = 50;
	CircularBuffer<unsigned long long, NUM_FRAME_DURATIONS> frameDurations;
	unsigned long long frameDurationSum;
	unsigned long long prevTimeStamp;
	
	// internal VDP counter, actually belongs in VDP
	int textModeCounter;

	Rasterizer* rasterizer;

	BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif //__PIXELRENDERER_HH__

