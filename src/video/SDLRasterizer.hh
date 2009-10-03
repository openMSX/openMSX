// $Id$

#ifndef SDLRASTERIZER_HH
#define SDLRASTERIZER_HH

#include "Rasterizer.hh"
#include "Observer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class Display;
class VDP;
class VDPVRAM;
class OutputSurface;
class VisibleSurface;
class RawFrame;
class RenderSettings;
class Setting;
class PostProcessor;
template <class Pixel> class CharacterConverter;
template <class Pixel> class BitmapConverter;
template <class Pixel> class SpriteConverter;

/** Rasterizer using a frame buffer approach: it writes pixels to a single
  * rectangular pixel buffer.
  */
template <class Pixel>
class SDLRasterizer : public Rasterizer, private noncopyable,
                      private Observer<Setting>
{
public:
	SDLRasterizer(
		VDP& vdp, Display& display, VisibleSurface& screen,
		std::auto_ptr<PostProcessor> postProcessor);
	virtual ~SDLRasterizer();

	// Rasterizer interface:
	virtual bool isActive();
	virtual void reset();
	virtual void frameStart(EmuTime::param time);
	virtual void frameEnd();
	virtual void setDisplayMode(DisplayMode mode);
	virtual void setPalette(int index, int grb);
	virtual void setBackgroundColor(int index);
	virtual void setTransparency(bool enabled);
	virtual void setSuperimposing(bool enabled);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight);
	virtual void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight);
	virtual bool isRecording() const;

private:
	/** Translate from absolute VDP coordinates to screen coordinates:
	  * Note: In reality, there are only 569.5 visible pixels on a line.
	  *       Because it looks better, the borders are extended to 640.
	  * @param absoluteX Absolute VDP coordinate.
	  * @param narrow Is this a narrow (512 pixels wide) display mode?
	  */
	inline static int translateX(int absoluteX, bool narrow);

	inline void renderBitmapLine(Pixel* buf, unsigned vramLine);

	/** Reload entire palette from VDP.
	  */
	void resetPalette();

	/** Precalc palette values.
	  * For MSX1 VDPs, results go directly into palFg/palBg.
	  * For higher VDPs, results go into V9938_COLORS and V9958_COLORS.
	  */
	void precalcPalette();
	Pixel calcColorHelper(double r, double g, double b, Pixel extra);

	/** Precalc foreground color index 0 (palFg[0]).
	  * @param mode Current display mode.
	  * @param transparency True iff transparency is enabled.
	  */
	void precalcColorIndex0(DisplayMode mode, bool transparency,
	                         bool superimposing, byte bgcolorIndex);

	// Observer<Setting>
	virtual void update(const Setting& setting);

	/** The VDP of which the video output is being rendered.
	  */
	VDP& vdp;

	/** The VRAM whose contents are rendered.
	  */
	VDPVRAM& vram;

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

	/** VRAM to pixels converter for character display modes.
	  */
	const std::auto_ptr<CharacterConverter<Pixel> > characterConverter;

	/** VRAM to pixels converter for bitmap display modes.
	  */
	const std::auto_ptr<BitmapConverter<Pixel> > bitmapConverter;

	/** VRAM to pixels converter for sprites.
	  */
	const std::auto_ptr<SpriteConverter<Pixel> > spriteConverter;

	/** Line to render at top of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderTop;

	/** Host colors corresponding to each VDP palette entry.
	  * palFg has entry 0 set to the current background color.
	  *       The 16 first entries are for even pixels, the next 16 are for
	  *       odd pixels. Second part is only needed (and guaranteed to be
	  *       up-to-date) in Graphics5 mode.
	  * palBg has entry 0 set to black.
	  */
	Pixel palFg[16 * 2], palBg[16];

	/** Host colors corresponding to each Graphic 7 sprite color.
	  */
	Pixel palGraphic7Sprites[16];

	/** Precalculated host colors corresponding to each possible V9938 color.
	  * Used by updatePalette to adjust palFg and palBg.
	  */
	Pixel V9938_COLORS[8][8][8];

	/** Host colors corresponding to the 256 color palette of Graphic7.
	  * Used by BitmapConverter.
	  */
	Pixel PALETTE256[256];

	/** Host colors corresponding to each possible V9958 color.
	  */
	Pixel V9958_COLORS[32768];
};

} // namespace openmsx

#endif
