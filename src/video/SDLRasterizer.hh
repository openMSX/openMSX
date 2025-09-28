#ifndef SDLRASTERIZER_HH
#define SDLRASTERIZER_HH

#include "BitmapConverter.hh"
#include "CharacterConverter.hh"
#include "Rasterizer.hh"
#include "SpriteConverter.hh"

#include "Observer.hh"

#include <array>
#include <cstdint>
#include <memory>

namespace openmsx {

class Display;
class VDP;
class VDPVRAM;
class OutputSurface;
class RawFrame;
class RenderSettings;
class Setting;
class PostProcessor;

/** Rasterizer using a frame buffer approach: it writes pixels to a single
  * rectangular pixel buffer.
  */
class SDLRasterizer final : public Rasterizer
                          , private Observer<Setting>
{
public:
	using Pixel = uint32_t;

	SDLRasterizer(
		VDP& vdp, Display& display, OutputSurface& screen,
		std::unique_ptr<PostProcessor> postProcessor);
	SDLRasterizer(const SDLRasterizer&) = delete;
	SDLRasterizer(SDLRasterizer&&) = delete;
	SDLRasterizer& operator=(const SDLRasterizer&) = delete;
	SDLRasterizer& operator=(SDLRasterizer&&) = delete;
	~SDLRasterizer() override;

	// Rasterizer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	[[nodiscard]] bool isActive() override;
	void reset() override;
	void frameStart(EmuTime time) override;
	void frameEnd() override;
	void setDisplayMode(DisplayMode mode) override;
	void setPalette(unsigned index, int grb) override;
	void setBackgroundColor(uint8_t index) override;
	void setHorizontalAdjust(int adjust) override;
	void setHorizontalScrollLow(uint8_t scroll) override;
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
	[[nodiscard]] bool isRecording() const override;

private:
	inline void renderBitmapLine(std::span<Pixel> buf, unsigned vramLine);

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
	                        const RawFrame* superimposing, uint8_t bgcolorIndex);

	// Get the border color(s). These are 16bpp or 32bpp host pixels.
	std::pair<Pixel, Pixel> getBorderColors();

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
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
	CharacterConverter characterConverter;

	/** VRAM to pixels converter for bitmap display modes.
	  */
	BitmapConverter bitmapConverter;

	/** VRAM to pixels converter for sprites.
	  */
	SpriteConverter spriteConverter;

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
	std::array<Pixel, 16 * 2> palFg;
	std::array<Pixel, 16> palBg;

	/** Host colors corresponding to each Graphic 7 sprite color.
	  */
	std::array<Pixel, 16> palGraphic7Sprites;

	/** Precalculated host colors corresponding to each possible V9938 color.
	  * Used by updatePalette to adjust palFg and palBg.
	  */
	std::array<std::array<std::array<Pixel, 8>, 8>, 8> V9938_COLORS;

	/** Host colors corresponding to the 256 color palette of Graphic7.
	  * Used by BitmapConverter.
	  */
	std::array<Pixel, 256> PALETTE256;

	/** Host colors corresponding to each possible V9958 color.
	  */
	std::array<Pixel, 32768> V9958_COLORS;
};

} // namespace openmsx

#endif
