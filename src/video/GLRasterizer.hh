// $Id$

#ifndef GLRASTERIZER_HH
#define GLRASTERIZER_HH

#include "Rasterizer.hh"
#include "VideoLayer.hh"
#include "CharacterConverter.hh"
#include "BitmapConverter.hh"
#include "SpriteConverter.hh"
#include "DirtyChecker.hh"
#include "DisplayMode.hh"
#include "GLUtil.hh"
#include "openmsx.hh"

namespace openmsx {

class CommandController;
class RenderSettings;
class Display;
class OutputSurface;

/** Hi-res (640x480) rasterizer using OpenGL.
  */
class GLRasterizer : public Rasterizer, public VideoLayer
{
public:
	// TODO: Make private.
	// The reason it's public is that non-member functions in GLRasterizer.cc
	// are using this type.
	typedef GLuint Pixel;

	GLRasterizer(
		CommandController& commandController,
		VDP& vdp, Display& display, OutputSurface& screen
		);
	virtual ~GLRasterizer();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

	// Rasterizer interface:
	virtual bool isActive();
	virtual void reset();
	virtual void frameStart();
	virtual void frameEnd();
	virtual void setDisplayMode(DisplayMode mode);
	virtual void setPalette(int index, int grb);
	virtual void setBackgroundColour(int index);
	virtual void setTransparency(bool enabled);
	virtual void updateVRAMCache(int address);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight);
	virtual void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight);

private:
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

	/** Reload entire palette from VDP.
	  */
	void resetPalette();

	/** Precalc palette values.
	  * For MSX1 VDPs, results go directly into palFg/palBg.
	  * For higher VDPs, results go into V9938_COLOURS and V9958_COLOURS.
	  */
	void precalcPalette();

	/** Precalc foreground colour index 0 (palFg[0]).
	  * @param mode Current display mode.
	  * @param transparency True iff transparency is enabled.
	  */
	void precalcColourIndex0(DisplayMode mode, bool transparency,
	                         byte bgcolorIndex);

	// Observer<Setting>
	void update(const Setting& setting);
	
	/** Settings shared between all renderers
	 */
	RenderSettings& renderSettings;

	/** The VDP of which the video output is being rendered.
	  */
	VDP& vdp;

	/** The VRAM whose contents are rendered.
	  */
	VDPVRAM& vram;

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
	  *       The 16 first entries are for even pixels, the next 16 are for
	  *       odd pixels. Second part is only needed (and guaranteed to be
	  *       up-to-date) in Graphics5 mode.
	  * palBg has entry 0 set to black.
	  */
	Pixel palFg[16 * 2], palBg[16];

	/** RGB colours corresponding to each Graphic 7 sprite colour.
	  */
	Pixel palGraphic7Sprites[16];

	/** RGB colours of current sprite palette.
	  * Points to either palBg or palGraphic7Sprites.
	  */
	Pixel* palSprites;

	/** RGB colours corresponding to each possible V9938 colour.
	  * Used by updatePalette to adjust palFg and palBg.
	  */
	Pixel V9938_COLOURS[8][8][8];

	/** RGB colours corresponding to the 256 colour palette of Graphic7.
	  * Used by BitmapConverter.
	  */
	Pixel PALETTE256[256];

	/** RGB colours corresponding to each possible V9958 colour.
	  */
	Pixel V9958_COLOURS[32768];

	/** The surface which is visible to the user.
	  */
	OutputSurface& screen;

	/** Work area for redefining textures.
	  */
	Pixel lineBuffer[512];

	/** Cache for rendered VRAM in bitmap modes.
	  * Cache line N corresponds to VRAM at N * 128.
	  * It holds up to 4 pages of 256 lines each.
	  * In Graphics6/7 the lower two pages are used.
	  */
	BitmapTexture* bitmapTexture;

	/** One texture per absolute display line to draw sprite plane in.
	  * This is not an efficient way to draw sprites, but it was easy
	  * to implement. Will probably be replaced in the future.
	  */
	LineTexture spriteTextures[313];

	/** ID of texture that stores rendered frame.
	  * Used for effects and for putStoredImage.
	  */
	StoredFrame storedFrame;

	/** Is the frame buffer dirty?
	  */
	bool frameDirty;

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

	GLuint colorChrTex[4 * 256];
	GLuint monoChrTex[256];
	GLuint stripeTexture;

	/** VRAM to pixels converter for character display modes.
	  */
	CharacterConverter<Pixel> characterConverter;

	/** VRAM to pixels converter for bitmap display modes.
	  */
	BitmapConverter<Pixel> bitmapConverter;

	/** VRAM to pixels converter for sprites.
	  */
	SpriteConverter<Pixel> spriteConverter;
};

} // namespace openmsx

#endif
