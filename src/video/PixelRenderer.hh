// $Id$

#ifndef __PIXELRENDERER_HH__
#define __PIXELRENDERER_HH__

#include "openmsx.hh"
#include "Renderer.hh"
#include "VDPVRAM.hh"
#include "CircularBuffer.hh"
#include "SettingListener.hh"
#include "RenderSettings.hh"
#include "DisplayMode.hh"
#include "Event.hh"
#include "InfoTopic.hh"

namespace openmsx {

class VDP;
class SpriteChecker;


/** Abstract base class for pixel-based Renderers.
  * Provides a framework for implementing a pixel-based Renderer,
  * thereby reducing the amount of code needed to implement one.
  */
class PixelRenderer : public Renderer, protected SettingListener
{
public:
	// Renderer interface:

	void frameStart(const EmuTime &time);
	void frameEnd(const EmuTime &time);
	void updateDisplayEnabled(bool enabled, const EmuTime &time);
	void updateHorizontalScrollLow(byte scroll, const EmuTime &time);
	void updateHorizontalScrollHigh(byte scroll, const EmuTime &time);
	void updateBorderMask(bool masked, const EmuTime &time);
	void updateMultiPage(bool multiPage, const EmuTime &time);
	/*
	void setFullScreen(bool);
	void updateTransparency(bool enabled, const EmuTime &time);
	void updateForegroundColour(int colour, const EmuTime &time);
	void updateBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkForegroundColour(int colour, const EmuTime &time);
	void updateBlinkBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkState(bool enabled, const EmuTime &time);
	void updatePalette(int index, int grb, const EmuTime &time);
	void updateVerticalScroll(int scroll, const EmuTime &time);
	void updateHorizontalAdjust(int adjust, const EmuTime &time);
	void updateDisplayMode(DisplayMode mode, const EmuTime &time);
	void updateNameBase(int addr, const EmuTime &time);
	void updatePatternBase(int addr, const EmuTime &time);
	void updateColourBase(int addr, const EmuTime &time);
	*/
	void updateSpritesEnabled(bool enabled, const EmuTime &time);
	void updateVRAM(int offset, const EmuTime &time);
	void updateWindow(bool enabled, const EmuTime &time);

protected:
	/** Constructor.
	  */
	PixelRenderer(RendererFactory::RendererID id, VDP *vdp);

	/** Destructor.
	  */
	virtual ~PixelRenderer();

	virtual void reset(const EmuTime &time);

	/** Let underlying graphics system finish rendering this frame.
	  * The image is stored for plotting later, once or multiple times,
	  * using the putImage method.
	  */
	virtual void finishFrame() = 0;

	/** Render a rectangle of border pixels on the host screen.
	  * The units are absolute lines (Y) and VDP clockticks (X).
	  * @param fromX X coordinate of render start (inclusive).
	  * @param fromY Y coordinate of render start (inclusive).
	  * @param limitX X coordinate of render end (exclusive).
	  * @param limitY Y coordinate of render end (exclusive).
	  */
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY) = 0;

	/** Render a rectangle of display pixels on the host screen.
	  * @param fromX X coordinate of render start in VDP ticks.
	  * @param fromY Y coordinate of render start in absolute lines.
	  * @param displayX display coordinate of render start: [0..512).
	  * @param displayY display coordinate of render start: [0..256).
	  * @param displayWidth rectangle width in pixels (512 per line).
	  * @param displayHeight rectangle height in lines.
	  */
	virtual void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		) = 0;

	/** Render a rectangle of sprite pixels on the host screen.
	  * Although the parameters are very similar to drawDisplay,
	  * the displayX and displayWidth use range [0..256) instead of
	  * [0..512) because VDP sprite coordinates work that way.
	  * @param fromX X coordinate of render start in VDP ticks.
	  * @param fromY Y coordinate of render start in absolute lines.
	  * @param displayX display coordinate of render start: [0..256).
	  * @param displayY display coordinate of render start: [0..256).
	  * @param displayWidth rectangle width in pixels (256 per line).
	  * @param displayHeight rectangle height in lines.
	  */
	virtual void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		) = 0;

	/** Notifies the VRAM cache of a VRAM write.
	  */
	virtual void updateVRAMCache(int address) = 0;

	// SettingListener:
	virtual void update(const SettingLeafNode* setting) throw();

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

	/** The VDP of which the video output is being rendered.
	  */
	VDP *vdp;

	/** The VRAM whose contents are rendered.
	  */
	VDPVRAM *vram;

	/** The sprite checker whose sprites are rendered.
	  */
	SpriteChecker *spriteChecker;

	/** This event gets distibuted when a frame is rendered.
	 */
	SimpleEvent<FINISH_FRAME_EVENT> finishFrameEvent;

private:
	/** Indicates whether the area to be drawn is border or display. */
	enum DrawType { DRAW_BORDER, DRAW_DISPLAY, DRAW_SPRITES };

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

	/** Render lines until specified moment in time.
	  * Unlike sync(), this method does not sync with VDPVRAM.
	  * The VRAM should be to be up to date and remain unchanged
	  * from the current time to the specified time.
	  * @param time Moment in emulated time to render lines until.
	  */
	void renderUntil(const EmuTime &time);

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
	unsigned finishFrameDuration;
	
	static const unsigned NUM_FRAME_DURATIONS = 50;
	CircularBuffer<unsigned, NUM_FRAME_DURATIONS> frameDurations;
	unsigned frameDurationSum;
	unsigned prevTimeStamp;
	
	class FpsInfoTopic : public InfoTopic {
	public:
		FpsInfoTopic(PixelRenderer& parent);
		virtual void execute(const vector<string>& tokens,
		                     CommandResult& result) const throw();
		virtual string help (const vector<string>& tokens) const
			throw();
	private:
		PixelRenderer& parent;
	} fpsInfo;

	// internal VDP counter, actually belongs in VDP
	int textModeCounter;
};

} // namespace openmsx

#endif //__PIXELRENDERER_HH__

