// $Id$

#ifndef __SDLGLRENDERER_HH__
#define __SDLGLRENDERER_HH__

#include "GLUtil.hh"
#ifdef __OPENGL_AVAILABLE__

#include <SDL/SDL.h>
#include "openmsx.hh"
#include "PixelRenderer.hh"
#include "CharacterConverter.hh"
#include "BitmapConverter.hh"
#include "SpriteConverter.hh"
#include "DirtyChecker.hh"
#include "DisplayMode.hh"

class OSDConsoleRenderer;


/** Hi-res (640x480) renderer on SDL.
  */
class SDLGLRenderer : public PixelRenderer
{
public:

	// TODO: Make private.
	// The reason it's public is that non-member functions in SDLGLRenderer.cc
	// are using this type.
	typedef GLuint Pixel;

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
	void updateVRAMCache(int address);
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

	friend class SDLGLRendererFactory;

	/** Constructor, called by SDLGLRendererFactory.
	  */
	SDLGLRenderer(
		RendererFactory::RendererID id, VDP *vdp, SDL_Surface *screen );

	/** Destructor.
	  */
	virtual ~SDLGLRenderer();

	inline void renderBitmapLine(byte mode, int vramLine);
	inline void renderBitmapLines(byte line, int count);
	inline void renderPlanarBitmapLine(byte mode, int vramLine);
	inline void renderPlanarBitmapLines(byte line, int count);
	inline void renderCharacterLines(byte line, int count);

	inline void renderText1(
		int vramLine, int screenLine, int count, int minX, int maxX );
	inline void renderText2(
		int vramLine, int screenLine, int count, int minX, int maxX );
	inline void renderGraphic2(
		int vramLine, int screenLine, int count, int minX, int maxX );
	inline void renderGraphic2Row(
		int row, int screenLine, int col, int endCol );
	inline void renderMultiColour(
		int vramLine, int screenLine, int count, int minX, int maxX );

	/** Get a pointer to the start of a VRAM line in the cache.
	  * @param displayCache The display cache to use.
	  * @param line The VRAM line, range depends on display cache.
	  */
	inline Pixel *getLinePtr(Pixel *displayCache, int line);

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

	/** Apply effects such as scanline or blur to the current image.
	  * For blur, the image in the texture identified by storedImageTextureId
	  * is used to blur over the current image.
	  * @param blurSetting Amount of horizontal blur to apply,
	  * 	0 <= blurSetting <= 100.
	  * @param scanlineAlpha Alpha value for scanlines,
	  * 	0 <= scanlineAlpha <= 255, 0 = transparent, 255 = opaque.
	  */
	void drawEffects(int blurSetting, int scanlineAlpha);

	/** RGB colours corresponding to each VDP palette entry.
	  * palFg has entry 0 set to the current background colour,
	  * palBg has entry 0 set to black.
	  */
	Pixel palFg[16], palBg[16];

	/** RGB colours corresponding to each Graphic 7 sprite colour.
	  */
	Pixel palGraphic7Sprites[16];

	/** RGB colours of current sprite palette.
	  * Points to either palBg or palGraphic7Sprites.
	  */
	Pixel *palSprites;

	/** RGB colours corresponding to each possible V9938 colour.
	  * Used by updatePalette to adjust palFg and palBg.
	  */
	Pixel V9938_COLOURS[8][8][8];

	/** RGB colours corresponding to the 256 colour palette of Graphic7.
	  * Used by BitmapConverter.
	  */
	Pixel PALETTE256[256];

	/** SDL colours corresponding to each possible V9958 colour.
	  */
	Pixel V9958_COLOURS[32768];

	/** The surface which is visible to the user.
	  */
	SDL_Surface *screen;

	/** Work area for redefining textures.
	  */
	Pixel lineBuffer[512];

	/** Cache for rendered VRAM in character modes.
	  * Cache line (N + scroll) corresponds to display line N.
	  * It holds a single page of 256 lines.
	  */
	GLuint charTextureIds[256];

	/** Cache for rendered VRAM in bitmap modes.
	  * Cache line N corresponds to VRAM at N * 128.
	  * It holds up to 4 pages of 256 lines each.
	  * In Graphics6/7 the lower two pages are used.
	  */
	GLuint bitmapTextureIds[4 * 256];

	/** Test.
	  */
	GLuint spriteTextureIds[313];

	/** ID of texture that stores rendered frame.
	  * Used for effects and for putStoredImage.
	  */
	GLuint storedImageTextureId;

	/** Was the previous frame image stored?
	  */
	bool prevStored;

	/** Previous value of gamma setting.
	  */
	float prevGamma;

	/** Display mode the line is valid in.
	  * 0xFF means invalid in every mode.
	  */
	byte lineValidInMode[256 * 4];

	/** Line to render at top of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderTop;

	/** Number of host pixels per line.
	  * In Graphic 5 and 6 this is 512, in all other modes it is 256.
	  */
	int lineWidth;

	/** Display mode for which character cache is valid.
	  * This is used to speed up bitmap/character mode splits,
	  * like Space Manbow and Psycho World use.
	  */
	DisplayMode characterCacheMode;
	/** Dirty checker for pattern table. */
	DirtyChecker<8> dirtyPattern;
	/** Dirty checker for colour table. */
	DirtyChecker<8> dirtyColour;

	GLuint characterCache[4 * 256];

	/** VRAM to pixels converter for character display modes.
	  */
	CharacterConverter<Pixel, Renderer::ZOOM_REAL> characterConverter;

	/** VRAM to pixels converter for bitmap display modes.
	  */
	BitmapConverter<Pixel, Renderer::ZOOM_REAL> bitmapConverter;

	/** VRAM to pixels converter for sprites.
	  */
	SpriteConverter<Pixel, Renderer::ZOOM_REAL> spriteConverter;

	OSDConsoleRenderer *console;
	OSDConsoleRenderer *debugger;

};

#endif // __OPENGL_AVAILABLE__
#endif // __SDLGLRENDERER_HH__
