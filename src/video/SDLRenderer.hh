// $Id$

#ifndef __SDLRENDERER_HH__
#define __SDLRENDERER_HH__

#include <SDL/SDL.h>
#include "openmsx.hh"
#include "PixelRenderer.hh"
#include "CharacterConverter.hh"
#include "BitmapConverter.hh"
#include "SpriteConverter.hh"
#include "DisplayMode.hh"

class OSDConsoleRenderer;


/** Renderer on SDL surface.
  */
template <class Pixel, Renderer::Zoom zoom>
class SDLRenderer : public PixelRenderer
{
public:

	// Renderer interface:

	void reset(const EmuTime &time);
	bool checkSettings();
	void frameStart(const EmuTime &time);
	//void putImage(const EmuTime &time);
	void updateTransparency(bool enabled, const EmuTime &time);
	void updateForegroundColour(int colour, const EmuTime &time);
	void updateBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkForegroundColour(int colour, const EmuTime &time);
	void updateBlinkBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkState(bool enabled, const EmuTime &time);
	void updatePalette(int index, int grb, const EmuTime &time);
	void updateVerticalScroll(int scroll, const EmuTime &time);
	void updateHorizontalAdjust(int adjust, const EmuTime &time);
	//void updateDisplayEnabled(bool enabled, const EmuTime &time);
	void updateDisplayMode(DisplayMode mode, const EmuTime &time);
	void updateNameBase(int addr, const EmuTime &time);
	void updatePatternBase(int addr, const EmuTime &time);
	void updateColourBase(int addr, const EmuTime &time);
	//void updateVRAM(int offset, const EmuTime &time);
	//void updateWindow(bool enabled, const EmuTime &time);

protected:
	void finishFrame(bool store);
	void putStoredImage();
	void updateVRAMCache(int addr);
	void drawBorder(int fromX, int fromY, int limitX, int limitY);
	void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		);
	void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		);

private:

	/** Horizontal dimensions of the screen.
	  */
	static const int WIDTH = (zoom == Renderer::ZOOM_256 ? 320 : 640);

	/** Vertical dimensions of the screen.
	  */
	static const int HEIGHT = (zoom == Renderer::ZOOM_256 ? 240 : 480);

	/** Number of host screen lines per VDP display line.
	  */
	static const int LINE_ZOOM = (zoom == Renderer::ZOOM_256 ? 1 : 2);

	friend class SDLLoRendererFactory;
	friend class SDLHiRendererFactory;

	/** Translate from absolute VDP coordinates to screen coordinates:
	  * Note: In reality, there are only 569.5 visible pixels on a line.
	  *       Because it looks better, the borders are extended to 640.
	  */
	inline static int translateX(int absoluteX);

	/** Constructor, called by SDL(Hi/Lo)RendererFactory.
	  */
	SDLRenderer(RendererFactory::RendererID id, VDP *vdp, SDL_Surface *screen);

	/** Destructor.
	  */
	virtual ~SDLRenderer();

	inline void renderBitmapLine(byte mode, int vramLine);
	inline void renderBitmapLines(byte line, int count);
	inline void renderPlanarBitmapLine(byte mode, int vramLine);
	inline void renderPlanarBitmapLines(byte line, int count);
	inline void renderCharacterLines(byte line, int count);

	/** Get a pointer to the start of a VRAM line in the cache.
	  * @param displayCache The display cache to use.
	  * @param line The VRAM line, range depends on display cache.
	  */
	inline Pixel *getLinePtr(SDL_Surface *displayCache, int line);

	/** Get the pixel colour of a graphics 7 colour index.
	  */
	inline Pixel graphic7Colour(byte index);

	/** Get the pixel colour of the border.
	  * SCREEN6 has separate even/odd pixels in the border.
	  * TODO: Implement the case that even_colour != odd_colour.
	  */
	inline Pixel getBorderColour();

	/** Precalc palette values.
	  * For MSX1 VDPs, results go directly into palFg/palBg.
	  * For higher VDPs, results go into V9938_COLOURS and V9958_COLOURS.
	  * @param gamma Gamma correction factor.
	  */
	void precalcPalette(float gamma);

	/** Precalc several values that depend on the display mode.
	  * @param mode The new display mode.
	  */
	void setDisplayMode(DisplayMode mode);

	/** Reload entire palette from VDP.
	  */
	void resetPalette();

	/** Change an entry in the palette.
	  * Used to implement updatePalette.
	  */
	void setPalette(int index, int grb);

	/** Line to render at top of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderTop;

	/** Line to render at bottom of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderBottom;

	/** SDL colours corresponding to each VDP palette entry.
	  * palFg has entry 0 set to the current background colour,
	  * palBg has entry 0 set to black.
	  */
	Pixel palFg[16], palBg[16];

	/** SDL colours corresponding to each Graphic 7 sprite colour.
	  */
	Pixel palGraphic7Sprites[16];

	/** SDL colours corresponding to each possible V9938 colour.
	  * Used by updatePalette to adjust palFg and palBg.
	  * Since SDL_MapRGB may be slow, this array stores precalculated
	  * SDL colours for all possible RGB values.
	  */
	Pixel V9938_COLOURS[8][8][8];

	/** SDL colours corresponding to the 256 colour palette of Graphic7.
	  * Used by BitmapConverter.
	  */
	Pixel PALETTE256[256];

	/** SDL colours corresponding to each possible V9958 colour.
	  */
	Pixel V9958_COLOURS[32768];
	
	/** The surface which is visible to the user.
	  */
	SDL_Surface *screen;

	/** The stored image, see putImage and putStoredImage.
	  */
	SDL_Surface *storedImage;

	/** Cache for rendered VRAM in character modes.
	  * Cache line (N + scroll) corresponds to display line N.
	  * It holds a single page of 256 lines.
	  */
	SDL_Surface *charDisplayCache;

	/** Cache for rendered VRAM in bitmap modes.
	  * Cache line N corresponds to VRAM at N * 128.
	  * It holds up to 4 pages of 256 lines each.
	  * In Graphics6/7 the lower two pages are used.
	  */
	SDL_Surface *bitmapDisplayCache;

	/** Previous value of gamma setting.
	  */
	float prevGamma;

	/** Display mode the line is valid in.
	  * 0xFF means invalid in every mode.
	  */
	byte lineValidInMode[256 * 4];

	/** VRAM to pixels converter for character display modes.
	  */
	CharacterConverter<Pixel, zoom> characterConverter;

	/** VRAM to pixels converter for bitmap display modes.
	  */
	BitmapConverter<Pixel, zoom> bitmapConverter;

	/** VRAM to pixels converter for sprites.
	  */
	SpriteConverter<Pixel, zoom> spriteConverter;

	OSDConsoleRenderer *console;
};

#endif //__SDLRENDERER_HH__

