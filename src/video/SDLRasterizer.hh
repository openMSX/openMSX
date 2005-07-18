// $Id$

#ifndef SDLRASTERIZER_HH
#define SDLRASTERIZER_HH

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
class RawFrame;


/** Rasterizer using SDL.
  */
template <class Pixel, Renderer::Zoom zoom>
class SDLRasterizer : public Rasterizer
{
public:
	/** Constructor.
	  */
	SDLRasterizer(VDP& vdp, SDL_Surface* screen);

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
	// SettingListener interface:
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
	void precalcColourIndex0(DisplayMode mode, bool transparency,
	                         byte bgcolorIndex);

	/** (Re)initialize the "abcdFrame" variables.
	  * @param first True iff this is the first call.
	  *   The first call should be done by the constructor.
	  */
	void initFrames(bool first = false);

	/** Sets up the "abcdFrame" variables for a new frame.
	  * TODO: The point of passing the finished frame in and the new workFrame
	  *       out is to be able to split off the scaler application as a
	  *       separate class.
	  * @param finishedFrame Frame that has just become available.
	  * @param oddField Display odd field (true) or even field (false).
	  * @return RawFrame object that can be used for building the next frame.
	  */
	RawFrame* rotateFrames(RawFrame* finishedFrame, bool oddField);

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
	  * palFg has entry 0 set to the current background colour.
	  *       The 16 first entries are for even pixels, the next 16 are for
	  *       odd pixels. Second part is only needed (and guaranteed to be
	  *       up-to-date) in Graphics5 mode.
	  * palBg has entry 0 set to black.
	  */
	Pixel palFg[16 * 2], palBg[16];

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

	/** The next frame as it is delivered by the VDP, work in progress.
	  */
	RawFrame* workFrame;

	/** The last finished frame, ready to be displayed.
	  */
	RawFrame* currFrame;

	/** The frame before currFrame, ready to be displayed.
	  */
	RawFrame* prevFrame;

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

	/** Blend to pixel colors
	 */
	Blender<Pixel> blender;

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

#endif
