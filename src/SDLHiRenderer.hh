// $Id$

#ifndef __SDLHIRENDERER_HH__
#define __SDLHIRENDERER_HH__

#include <SDL/SDL.h>
#include "openmsx.hh"
#include "Renderer.hh"


class VDP;

/** Factory method to create SDLHiRenderer objects.
  * TODO: Add NTSC/PAL selection
  *   (immutable because it is colour encoding, not refresh frequency).
  */
Renderer *createSDLHiRenderer(VDP *vdp, bool fullScreen, const EmuTime &time);

/** Hi-res (640x480) renderer on SDL.
  */
template <class Pixel> class SDLHiRenderer : public Renderer
{
public:
	/** Constructor.
	  * It is suggested to use the createSDLHiRenderer factory
	  * function instead, which automatically selects a colour depth.
	  */
	SDLHiRenderer(VDP *vdp, SDL_Surface *screen, const EmuTime &time);

	/** Destructor.
	  */
	virtual ~SDLHiRenderer();

	// Renderer interface:

	void putImage();
	void setFullScreen(bool);
	void updateTransparency(bool enabled, const EmuTime &time);
	void updateForegroundColour(int colour, const EmuTime &time);
	void updateBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkState(bool enabled, const EmuTime &time);
	void updatePalette(int index, int grb, const EmuTime &time);
	void updateDisplayEnabled(bool enabled, const EmuTime &time);
	void updateDisplayMode(int mode, const EmuTime &time);
	void updateNameBase(int addr, const EmuTime &time);
	void updatePatternBase(int addr, const EmuTime &time);
	void updateColourBase(int addr, const EmuTime &time);
	void updateSpriteAttributeBase(int addr, const EmuTime &time);
	void updateSpritePatternBase(int addr, const EmuTime &time);
	void updateVRAM(int addr, byte data, const EmuTime &time);

private:
	typedef void (SDLHiRenderer::*RenderMethod)(int line);
	typedef void (SDLHiRenderer::*PhaseHandler)(int limit);
	typedef void (SDLHiRenderer::*DirtyChecker)(int addr, byte data);

	void renderUntil(int limit);

	void renderText1(int line);
	void renderText1Q(int line);
	void renderText2(int line);
	void renderGraphic1(int line);
	void renderGraphic2(int line);
	void renderGraphic4(int line);
	void renderGraphic5(int line);
	void renderMulti(int line);
	void renderMultiQ(int line);
	void renderBogus(int line);

	void offPhase(int limit);
	void blankPhase(int limit);
	void displayPhase(int limit);

	/** Dirty checking that does nothing (but is a valid method).
	  */
	void checkDirtyNull(int addr, byte data);

	/** Dirty checking for MSX1 display modes.
	  */
	void checkDirtyMSX1(int addr, byte data);

	/** Dirty checking for Text2 display mode.
	  */
	void checkDirtyText2(int addr, byte data);

	/** Draw sprites on this line over the background.
	  */
	void drawSprites(int absLine);

	/** Set all dirty / clean.
	  */
	void setDirty(bool);

	/** RenderMethods for each screen mode.
	  */
	static RenderMethod modeToRenderMethod[];

	/** DirtyCheckers for each screen mode.
	  */
	static DirtyChecker modeToDirtyChecker[];

	/** The VDP of which the video output is being rendered.
	  */
	VDP *vdp;

	/** SDL colours corresponding to each VDP palette entry.
	  * palFg has entry 0 set to the current background colour,
	  * palBg has entry 0 set to black.
	  */
	Pixel palFg[16], palBg[16];

	/** SDL colours corresponding to each possible V9938 colour.
	  * Used by updatePalette to adjust palFg and palBg.
	  * Since SDL_MapRGB may be slow, this array stores precalculated
	  * SDL colours for all possible RGB values.
	  */
	Pixel V9938_COLOURS[8][8][8];

	/** Rendering method for the current display mode.
	  */
	RenderMethod renderMethod;

	/** Phase handler: current drawing mode (off, blank, display).
	  */
	PhaseHandler phaseHandler;

	/** Dirty checker: update dirty tables on VRAM write.
	  */
	DirtyChecker dirtyChecker;

	/** Number of the next line to render.
	  * Absolute NTSC line number: [0..262).
	  */
	int currLine;

	/** The surface which is visible to the user.
	  */
	SDL_Surface *screen;

	/** The surface which the image is rendered on.
	  */
	SDL_Surface *displayCache;

	/** Pointers to the start of each display line in the cache.
	  */
	Pixel *cacheLinePtrs[212];

	/** Absolute line number of first display line.
	  */
	int lineDisplay;

	/** Absolute line number of first bottom border line.
	  */
	int lineBottomBorder;

	/** Dirty tables indicate which character blocks must be repainted.
	  * The anyDirty variables are true when there is at least one
	  * element in the dirty table that is true.
	  */
	bool anyDirtyColour, dirtyColour[1 << 10];
	bool anyDirtyPattern, dirtyPattern[1 << 10];
	bool anyDirtyName, dirtyName[1 << 12];
	// TODO: Introduce "allDirty" to speed up screen splits.

	/** Did foreground colour change since last screen update?
	  */
	bool dirtyForeground;

	/** Did background colour change since last screen update?
	  */
	bool dirtyBackground;

};

#endif //__SDLHIRENDERER_HH__

