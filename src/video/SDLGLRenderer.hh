// $Id$

#ifndef __SDLGLRENDERER_HH__
#define __SDLGLRENDERER_HH__

#include "GLUtil.hh"

#include <SDL.h>
#include "openmsx.hh"
#include "PixelRenderer.hh"
#include "CharacterConverter.hh"
#include "BitmapConverter.hh"
#include "SpriteConverter.hh"
#include "DirtyChecker.hh"
#include "DisplayMode.hh"

namespace openmsx {

class OSDConsoleRenderer;


/** Hi-res (640x480) renderer using OpenGL through SDL.
  */
class SDLGLRenderer : public PixelRenderer
{
public:
	// TODO: Make private.
	// The reason it's public is that non-member functions in SDLGLRenderer.cc
	// are using this type.
	typedef GLuint Pixel;

	// Renderer interface:

	virtual void reset(const EmuTime& time);
	virtual bool checkSettings();
	virtual void frameStart(const EmuTime& time);
	//virtual void frameEnd(const EmuTime& time);
	virtual void putImage();
	virtual int  putPowerOffImage();
	virtual void takeScreenShot(const string& filename)
		throw(CommandException);
	virtual void updateTransparency(bool enabled, const EmuTime& time);
	virtual void updateForegroundColour(int colour, const EmuTime& time);
	virtual void updateBackgroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkForegroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkBackgroundColour(int colour, const EmuTime& time);
	virtual void updateBlinkState(bool enabled, const EmuTime& time);
	virtual void updatePalette(int index, int grb, const EmuTime& time);
	virtual void updateVerticalScroll(int scroll, const EmuTime& time);
	virtual void updateHorizontalAdjust(int adjust, const EmuTime& time);
	//virtual void updateDisplayEnabled(bool enabled, const EmuTime& time);
	virtual void updateDisplayMode(DisplayMode mode, const EmuTime& time);
	virtual void updateNameBase(int addr, const EmuTime& time);
	virtual void updatePatternBase(int addr, const EmuTime& time);
	virtual void updateColourBase(int addr, const EmuTime& time);
	//virtual void updateVRAM(int offset, const EmuTime& time);
	//virtual void updateWindow(bool enabled, const EmuTime& time);

protected:
	void finishFrame();
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

	inline void renderText1(
		int vramLine, int screenLine, int count, int minX, int maxX );
	inline void renderText2(
		int vramLine, int screenLine, int count, int minX, int maxX );
	inline void renderGraphic1(
		int vramLine, int screenLine, int count, int minX, int maxX );
	inline void renderGraphic1Row(
		int row, int screenLine, int col, int endCol );
	inline void renderGraphic2(
		int vramLine, int screenLine, int count, int minX, int maxX );
	inline void renderGraphic2Row(
		int row, int screenLine, int col, int endCol );
	inline void renderMultiColour(
		int vramLine, int screenLine, int count, int minX, int maxX );

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

	/** Precalc foreground colour index 0 (palFg[0]).
	  * @param mode Current display mode.
	  * @param transparency True iff transparency is enabled.
	  */
	void precalcColourIndex0(DisplayMode mode, bool transparency = true);

	/** TODO: Temporary method to reduce code duplication.
	  */
	void drawRest();

	/** Apply effects such as scanline or blur to the current image.
	  * Requires the image of the current frame to be stored.
	  */
	void drawEffects();

	/** Line to render at top of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderTop;

	/** Number of host pixels per line.
	  * In Graphic 5 and 6 this is 512, in all other modes it is 256.
	  */
	int lineWidth;

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

	/** Cache for rendered VRAM in bitmap modes.
	  * Cache line N corresponds to VRAM at N * 128.
	  * It holds up to 4 pages of 256 lines each.
	  * In Graphics6/7 the lower two pages are used.
	  */
	LineTexture *bitmapTextures;

	/** One texture per absolute display line to draw sprite plane in.
	  * This is not an efficient way to draw sprites, but it was easy
	  * to implement. Will probably be replaced in the future.
	  */
	LineTexture spriteTextures[313];

	/** ID of texture that stores rendered frame.
	  * Used for effects and for putStoredImage.
	  */
	GLuint storedImageTextureId;
	GLuint noiseTextureId;

	/** Was the previous frame image stored?
	  */
	bool prevStored;

	/** Is the frame buffer dirty?
	  */
	bool frameDirty;

	/** Previous value of gamma setting.
	  */
	float prevGamma;

	/** Display mode the line is valid in.
	  * 0xFF means invalid in every mode.
	  */
	byte lineValidInMode[256 * 4];

	/** Display mode for which character cache is valid.
	  * This is used to speed up bitmap/character mode splits,
	  * like Space Manbow and Psycho World use.
	  */
	DisplayMode characterCacheMode;
	/** Dirty checker for pattern table. */
	DirtyChecker<(1<<10), 8> dirtyPattern;
	/** Dirty checker for colour table. */
	DirtyChecker<(1<<10), 8> dirtyColour;

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
};

} // namespace openmsx

#endif // __SDLGLRENDERER_HH__
