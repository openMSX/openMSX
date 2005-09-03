// $Id$

#ifndef PIXELRENDERER_HH
#define PIXELRENDERER_HH

#include "Renderer.hh"
#include "SettingListener.hh"
#include "RenderSettings.hh"
#include "DisplayMode.hh"
#include "openmsx.hh"

namespace openmsx {

class EventDistributor;
class RealTime;
class Display;
class Rasterizer;
class VDP;
class SpriteChecker;
class VDPVRAM;

/** Generic implementation of a pixel-based Renderer.
  * Uses a Rasterizer to plot actual pixels for a specific video system.
  */
class PixelRenderer : public Renderer, private SettingListener
{
public:
	PixelRenderer(VDP& vdp);
	virtual ~PixelRenderer();

	// Renderer interface:
	virtual void reset(const EmuTime& time);
	virtual void frameStart(const EmuTime& time);
	virtual void frameEnd(const EmuTime& time);
	virtual void updateHorizontalScrollLow(byte scroll, const EmuTime& time);
	virtual void updateHorizontalScrollHigh(byte scroll, const EmuTime& time);
	virtual void updateBorderMask(bool masked, const EmuTime& time);
	virtual void updateMultiPage(bool multiPage, const EmuTime& time);
	virtual void updateTransparency(bool enabled, const EmuTime& time);
	virtual void updateForegroundColour(int colour, const EmuTime& time);
	virtual void updateBackgroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkForegroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkBackgroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkState(bool enabled, const EmuTime& time);
	virtual void updatePalette(int index, int grb, const EmuTime& time);
	virtual void updateVerticalScroll(int scroll, const EmuTime& time);
	virtual void updateHorizontalAdjust(int adjust, const EmuTime& time);
	virtual void updateDisplayEnabled(bool enabled, const EmuTime& time);
	virtual void updateDisplayMode(DisplayMode mode, const EmuTime& time);
	virtual void updateNameBase(int addr, const EmuTime& time);
	virtual void updatePatternBase(int addr, const EmuTime& time);
	virtual void updateColourBase(int addr, const EmuTime& time);
	virtual void updateSpritesEnabled(bool enabled, const EmuTime& time);
	virtual void updateVRAM(unsigned offset, const EmuTime& time);
	virtual void updateWindow(bool enabled, const EmuTime& time);

private:
	/** Indicates whether the area to be drawn is border or display. */
	enum DrawType { DRAW_BORDER, DRAW_DISPLAY };

	// SettingListener interface:
	virtual void update(const Setting* setting);

	/** Call the right draw method in the subclass,
	  * depending on passed drawType.
	  */
	void draw(
		int startX, int startY, int endX, int endY, DrawType drawType,
		bool atEnd);

	/** Subdivide an area specified by two scan positions into a series of
	  * rectangles.
	  * Clips the rectangles to { (x,y) | clipL <= x < clipR }.
	  * @param drawType
	  *   If DRAW_BORDER, draw rectangles using drawBorder;
	  *   if DRAW_DISPLAY, draw rectangles using drawDisplay.
	  */
	void subdivide(
		int startX, int startY, int endX, int endY,
		int clipL, int clipR, DrawType drawType );

	inline bool checkSync(int offset, const EmuTime& time);

	/** Update renderer state to specified moment in time.
	  * @param time Moment in emulated time to update to.
	  * @param force When screen accuracy is used,
	  * 	rendering is only performed if this parameter is true.
	  */
	void sync(const EmuTime& time, bool force = false);

	/** Render lines until specified moment in time.
	  * Unlike sync(), this method does not sync with VDPVRAM.
	  * The VRAM should be to be up to date and remain unchanged
	  * from the current time to the specified time.
	  * @param time Moment in emulated time to render lines until.
	  */
	void renderUntil(const EmuTime& time);

	/** The VDP of which the video output is being rendered.
	  */
	VDP& vdp;

	/** The VRAM whose contents are rendered.
	  */
	VDPVRAM& vram;

	EventDistributor& eventDistributor;
	RealTime& realTime;
	RenderSettings& renderSettings;
	
	/** The sprite checker whose sprites are rendered.
	  */
	SpriteChecker& spriteChecker;

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

	/** Should current frame be draw or can it be skipped.
	  */
	bool prevDrawFrame;
	bool drawFrame;
	bool renderFrame;

	int frameSkipCounter;
	double finishFrameDuration;

	// internal VDP counter, actually belongs in VDP
	int textModeCounter;

	Rasterizer* rasterizer;
};

} // namespace openmsx

#endif
