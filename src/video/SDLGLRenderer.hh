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
#include "DisplayMode.hh"

class GLConsole;


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
	//void updateVRAM(int address, const EmuTime &time);
	//void updateWindow(const EmuTime &time);

protected:
	void finishFrame();
	void updateVRAMCache(int addr) {
		(this->*dirtyChecker)(addr);
	}
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

	typedef void (SDLGLRenderer::*DirtyChecker)(int addr);

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
	inline void renderGraphic2(
		int vramLine, int screenLine, int count, int minX, int maxX );
	inline void renderGraphic2Row(
		int row, int screenLine, int col, int endCol );

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

	/** Precalc several values that depend on the display mode.
	  * @param mode The new display mode.
	  */
	void setDisplayMode(DisplayMode mode);

	/** Change an entry in the palette.
	  * Used to implement updatePalette.
	  */
	void setPalette(int index, int grb);

	/** Dirty checking that does nothing (but is a valid method).
	  */
	void checkDirtyNull(int addr);

	/** Dirty checking for MSX1 text modes.
	  */
	void checkDirtyMSX1Text(int addr);

	/** Dirty checking for MSX1 graphic modes.
	  */
	void checkDirtyMSX1Graphic(int addr);

	/** Dirty checking for Text2 display mode.
	  */
	void checkDirtyText2(int addr);

	/** Dirty checking for bitmap modes.
	  */
	void checkDirtyBitmap(int addr);

	/** Set all dirty / clean.
	  */
	void setDirty(bool);

	/** DirtyCheckers for each screen mode.
	  */
	static DirtyChecker modeToDirtyChecker[];

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

	/** Dirty checker: update dirty tables on VRAM write.
	  */
	DirtyChecker dirtyChecker;

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
	  * Used for blur effect.
	  */
	GLuint blurTextureId;

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

	/** Dirty tables indicate which character blocks must be repainted.
	  * The anyDirty variables are true when there is at least one
	  * element in the dirty table that is true.
	  */
	bool anyDirtyColour, dirtyColour[1 << 10];
	bool anyDirtyPattern, dirtyPattern[1 << 10];
	bool anyDirtyName, dirtyName[1 << 12];
	// TODO: Introduce "allDirty" to speed up screen splits.

	GLuint characterCache[4 * 256];

	/** Did foreground colour change since last screen update?
	  */
	bool dirtyForeground;

	/** Did background colour change since last screen update?
	  */
	bool dirtyBackground;

	/** VRAM to pixels converter for character display modes.
	  */
	CharacterConverter<Pixel, Renderer::ZOOM_REAL> characterConverter;

	/** VRAM to pixels converter for bitmap display modes.
	  */
	BitmapConverter<Pixel, Renderer::ZOOM_REAL> bitmapConverter;

	/** VRAM to pixels converter for sprites.
	  */
	SpriteConverter<Pixel, Renderer::ZOOM_REAL> spriteConverter;

	GLConsole* console;

};

#endif // __OPENGL_AVAILABLE__
#endif // __SDLGLRENDERER_HH__

