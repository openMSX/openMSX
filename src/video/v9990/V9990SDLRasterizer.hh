#ifndef V9990SDLRASTERIZER_HH
#define V9990SDLRASTERIZER_HH

#include "V9990Rasterizer.hh"
#include "V9990BitmapConverter.hh"
#include "V9990PxConverter.hh"
#include "Observer.hh"
#include <concepts>
#include <memory>

namespace openmsx {

class Display;
class V9990;
class V9990VRAM;
class RawFrame;
class OutputSurface;
class RenderSettings;
class Setting;
class PostProcessor;

/** Rasterizer using SDL.
  */
template<std::unsigned_integral Pixel>
class V9990SDLRasterizer final : public V9990Rasterizer
                               , private Observer<Setting>
{
public:
	V9990SDLRasterizer(
		V9990& vdp, Display& display, OutputSurface& screen,
		std::unique_ptr<PostProcessor> postProcessor);
	~V9990SDLRasterizer() override;
	V9990SDLRasterizer(const V9990SDLRasterizer&) = delete;
	V9990SDLRasterizer& operator=(const V9990SDLRasterizer&) = delete;

	// Rasterizer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	[[nodiscard]] bool isActive() override;
	void reset() override;
	void frameStart() override;
	void frameEnd(EmuTime::param time) override;
	void setDisplayMode(V9990DisplayMode displayMode) override;
	void setColorMode(V9990ColorMode colorMode) override;
	void setPalette(int index, byte r, byte g, byte b, bool ys) override;
	void setSuperimpose(bool enabled) override;
	void drawBorder(int fromX, int fromY, int limitX, int limitY) override;
	void drawDisplay(int fromX, int fromY, int toX, int toY,
	                 int displayX,
	                 int displayY, int displayYA, int displayYB) override;
	[[nodiscard]] bool isRecording() const override;

	/** Fill the palettes.
	  */
	void preCalcPalettes();
	void resetPalette();

	void drawP1Mode(int fromX, int fromY, int displayX,
	                int displayY, int displayYA, int displayYB,
	                int displayWidth, int displayHeight, bool drawSprites);
	void drawP2Mode(int fromX, int fromY, int displayX,
	                int displayY, int displayYA,
	                int displayWidth, int displayHeight, bool drawSprites);
	void drawBxMode(int fromX, int fromY, int displayX,
	                int displayY, int displayYA,
	                int displayWidth, int displayHeight, bool drawSprites);

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	/** screen width for SDLLo
	  */
	static constexpr int SCREEN_WIDTH  = 320;

	/** screenheight for SDLLo
	  */
	static constexpr int SCREEN_HEIGHT = 240;

	/** The VDP of which the video output is being rendered.
	  */
	V9990& vdp;

	/** The VRAM whose contents are rendered.
	  */
	V9990VRAM& vram;

	/** The surface which is visible to the user.
	  */
	OutputSurface& screen;

	/** The next frame as it is delivered by the VDP, work in progress.
	  */
	std::unique_ptr<RawFrame> workFrame;

	/** The current renderer settings (gamma, brightness, contrast)
	  */
	RenderSettings& renderSettings;

	/** Line to render at top of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderTop;

	/** First display column to draw.  Since the width of the VDP lines <=
	  * the screen width, colZero is <= 0. The non-displaying parts of the
	  * screen will be filled as border.
	  */
	int colZero;

	/** The current screen mode
	  */
	V9990DisplayMode displayMode;
	V9990ColorMode   colorMode;

	/** Palette containing the complete V9990 Color space
	  */
	Pixel palette32768[32768];

	/** The 256 color palette. A fixed subset of the palette32768.
	  */
	Pixel palette256[256];         // from index to host Pixel color
	int16_t palette256_32768[256]; // from index to 15bpp V9990 color
	// invariant: palette256[i] == palette32768[palette256_32768[i]]

	/** The 64 palette entries of the VDP - a subset of the palette32768.
	  * These are colors influenced by the palette IO ports and registers
	  */
	Pixel palette64[64];         // from index to host Pixel color
	int16_t palette64_32768[64]; // from index to 15bpp V9990 color
	// invariant: palette64[i] == palette32768[palette64_32768[i]]

	/** The video post processor which displays the frames produced by this
	  *  rasterizer.
	  */
	const std::unique_ptr<PostProcessor> postProcessor;

	V9990BitmapConverter<Pixel> bitmapConverter;
	V9990P1Converter<Pixel> p1Converter;
	V9990P2Converter<Pixel> p2Converter;
};

} // namespace openmsx

#endif
