// $Id$

#ifndef __SDLRASTERIZER_HH__
#define __SDLRASTERIZER_HH__

#include "Rasterizer.hh"
#include "CharacterConverter.hh"
#include "BitmapConverter.hh"
#include "SpriteConverter.hh"
#include "Scaler.hh"
#include "Deinterlacer.hh"
#include "openmsx.hh"
#include <memory>

struct SDL_Surface;

namespace openmsx {

template <class Pixel> class Scaler;
class VDP;
class VDPVRAM;


/** Rasterizer using SDL.
  */
template <class Pixel, Renderer::Zoom zoom>
class SDLRasterizer : public Rasterizer
{
public:
	/** Constructor.
	  */
	SDLRasterizer(VDP* vdp, SDL_Surface* screen);

	/** Destructor.
	  */
	virtual ~SDLRasterizer();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

	// Rasterizer interface:
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
		int displayWidth, int displayHeight
		);
	virtual void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		);

protected:
	// SettingListener interface:   TODO remove??
	virtual void update(const Setting* setting);

private:
	/** Horizontal dimensions of the screen.
	  */
	static const unsigned WIDTH = (zoom == Renderer::ZOOM_256 ? 320 : 640);

	/** Vertical dimensions of the screen.
	  */
	static const unsigned HEIGHT = (zoom == Renderer::ZOOM_256 ? 240 : 480);

	/** Number of host screen lines per VDP display line.
	  */
	static const int LINE_ZOOM = (zoom == Renderer::ZOOM_256 ? 1 : 2);

	/** Translate from absolute VDP coordinates to screen coordinates:
	  * Note: In reality, there are only 569.5 visible pixels on a line.
	  *       Because it looks better, the borders are extended to 640.
	  * @param absoluteX Absolute VDP coordinate.
	  * @param narrow Is this a narrow (512 pixels wide) display mode?
	  */
	inline static int translateX(int absoluteX, bool narrow);

	inline void renderBitmapLine(byte mode, int vramLine);
	inline void renderBitmapLines(byte line, int count);
	inline void renderPlanarBitmapLine(byte mode, int vramLine);
	inline void renderPlanarBitmapLines(byte line, int count);
	inline void renderCharacterLines(byte line, int count);

	/** Get a pointer to the start of a VRAM line in the cache.
	  * @param displayCache The display cache to use.
	  * @param line The VRAM line, range depends on display cache.
	  */
	inline Pixel* getLinePtr(SDL_Surface* displayCache, int line);

	/** Get the pixel colour of a graphics 7 colour index.
	  */
	inline Pixel graphic7Colour(byte index);

	/** Get the pixel colour of the border.
	  * SCREEN6 has separate even/odd pixels in the border.
	  * TODO: Implement the case that even_colour != odd_colour.
	  */
	inline Pixel getBorderColour();

	/** Reload entire palette from VDP.
	  */
	void resetPalette();

	/** Precalc palette values.
	  * For MSX1 VDPs, results go directly into palFg/palBg.
	  * For higher VDPs, results go into V9938_COLOURS and V9958_COLOURS.
	  * @param gamma Gamma correction factor.
	  */
	void precalcPalette(double gamma);

	/** Precalc foreground colour index 0 (palFg[0]).
	  * @param mode Current display mode.
	  * @param transparency True iff transparency is enabled.
	  */
	void precalcColourIndex0(DisplayMode mode, bool transparency = true);

	/** (Re)initialize workScreens array.
	  * @param first True iff this is the first call.
	  *   The first call should be done by the constructor.
	  */
	void initWorkScreens(bool first = false);

	/** Determine which surface to render to this frame;
	  * initializes workScreen pointer.
	  * @param oddField Display odd field (true) or even field (false).
	  */
	void calcWorkScreen(bool oddField);

	/** The VDP of which the video output is being rendered.
	  */
	VDP* vdp;

	/** The VRAM whose contents are rendered.
	  */
	VDPVRAM* vram;

	/** Line to render at top of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderTop;

	/** Remembers the type of pixels on a line.
	  * This is used to select the right scaler algorithm for a line.
	  */
	enum LineContent {
		/** Line contains border colour.
		  */
		LINE_BLANK,
		/** Line contains 256 (wide) pixels.
		  */
		LINE_256,
		/** Line contains 512 (narrow) pixels.
		  */
		LINE_512,
		/** Line does not need postprocessing.
		  */
		LINE_DONTTOUCH
	};
	LineContent lineContent[HEIGHT / LINE_ZOOM];

	/** Is the last completed frame interlaced?
	  * This is a copy of the VDP's interlace status,
	  * because the VDP keeps this status for the current frame.
	  * TODO: If GLRasterizer can use this as well,
	  *       it can be moved to Rasterizer?
	  */
	bool interlaced;

	/** The currently active scaler.
	  */
	std::auto_ptr<Scaler<Pixel> > currScaler;

	/** ID of the currently active scaler.
	  * Used to detect scaler changes.
	  */
	ScalerID currScalerID;

	Deinterlacer<Pixel> deinterlacer;

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
	SDL_Surface* screen;

	/** Points to the surface in workScreens that is used
	  * for the current frame.
	  */
	SDL_Surface* workScreen;

	/** Surfaces used in the render pipeline inbetween the caches and
	  * the visible screen.
	  * When the deinterlace feature is disabled, only index 0 is used.
	  * When it is enabled, index 0 is used for the even field and
	  * index 1 is used for the odd field.
	  * Unused entries are NULL.
	  */
	SDL_Surface* workScreens[2];

	/** Cache for rendered VRAM in character modes.
	  * Cache line (N + scroll) corresponds to display line N.
	  * It holds a single page of 256 lines.
	  */
	SDL_Surface* charDisplayCache;

	/** Cache for rendered VRAM in bitmap modes.
	  * Cache line N corresponds to VRAM at N * 128.
	  * It holds up to 4 pages of 256 lines each.
	  * In Graphics6/7 the lower two pages are used.
	  */
	SDL_Surface* bitmapDisplayCache;

	/** Previous value of gamma setting.
	  */
	double prevGamma;

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
};

} // namespace openmsx

#endif //__SDLRASTERIZER_HH__
