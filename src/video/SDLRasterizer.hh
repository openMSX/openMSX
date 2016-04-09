#ifndef SDLRASTERIZER_HH
#define SDLRASTERIZER_HH

#include "Rasterizer.hh"
#include "BitmapConverter.hh"
#include "CharacterConverter.hh"
#include "SpriteConverter.hh"
#include "Observer.hh"
#include "openmsx.hh"
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

/** Rasterizer using a frame buffer approach: it writes pixels to a single
  * rectangular pixel buffer.
  */
template <class Pixel>
class SDLRasterizer final : public Rasterizer
                          , private Observer<Setting>
{
public:
	SDLRasterizer(const SDLRasterizer&) = delete;
	SDLRasterizer& operator=(const SDLRasterizer&) = delete;

	SDLRasterizer(
		VDP& vdp, Display& display, VisibleSurface& screen,
		std::unique_ptr<PostProcessor> postProcessor);
	~SDLRasterizer();

	// Rasterizer interface:
	PostProcessor* getPostProcessor() const override;
	bool isActive() override;
	void reset() override;
	void frameStart(EmuTime::param time) override;
	void frameEnd() override;
	void setDisplayMode(DisplayMode mode) override;
	void setPalette(int index, int grb) override;
	void setBackgroundColor(int index) override;
	void setHorizontalAdjust(int adjust) override;
	void setHorizontalScrollLow(byte scroll) override;
	void setBorderMask(bool masked) override;
	void setTransparency(bool enabled) override;
	void setSuperimposeVideoFrame(const RawFrame* videoSource) override;
	void drawBorder(int fromX, int fromY, int limitX, int limitY) override;
	void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight) override;
	void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight) override;
	bool isRecording() const override;

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

	/** Precalc foreground color index 0 (palFg[0]).
	  * @param mode Current display mode.
	  * @param transparency True iff transparency is enabled.
	  */
	void precalcColorIndex0(DisplayMode mode, bool transparency,
	                        const RawFrame* superimposing, byte bgcolorIndex);

	// Some of the border-related settings changed.
	void borderSettingChanged();

	// Get the border color(s). These are 16bpp or 32bpp host pixels.
	void getBorderColors(Pixel& border0, Pixel& border1);

	// Observer<Setting>
	void update(const Setting& setting) override;

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
	const std::unique_ptr<PostProcessor> postProcessor;

	/** The next frame as it is delivered by the VDP, work in progress.
	  */
	std::unique_ptr<RawFrame> workFrame;

	/** The current renderer settings (gamma, brightness, contrast)
	  */
	RenderSettings& renderSettings;

	/** VRAM to pixels converter for character display modes.
	  */
	CharacterConverter<Pixel> characterConverter;

	/** VRAM to pixels converter for bitmap display modes.
	  */
	BitmapConverter<Pixel> bitmapConverter;

	/** VRAM to pixels converter for sprites.
	  */
	SpriteConverter<Pixel> spriteConverter;

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

	// True iff left/right border optimization can (still) be applied
	// this frame.
	bool canSkipLeftRightBorders;

	// True iff some of the left/right border related settings changed
	// during this frame (meaning the border pixels of this frame cannot
	// be reused for future frames).
	bool mixedLeftRightBorders;
};

} // namespace openmsx

#endif
