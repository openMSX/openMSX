// $Id$

#ifndef __SDLLORENDERER_HH__
#define __SDLLORENDERER_HH__

#include <SDL/SDL.h>
#include "openmsx.hh"
#include "Renderer.hh"


class MSXTMS9928a;

/** Factory method to create SDLLoRenderer objects.
  */
Renderer *createSDLLoRenderer(MSXTMS9928a *vdp, bool fullScreen);

/** Low-res (320x240) renderer on SDL.
  */
template <class Pixel> class SDLLoRenderer : public Renderer
{
public:
	/** Constructor.
	  * It is suggested to use the createSDLLoRenderer factory
	  * function instead, which automatically selects a colour depth.
	  */
	SDLLoRenderer(MSXTMS9928a *vdp, SDL_Surface *screen);
	virtual ~SDLLoRenderer();

	// Renderer interface:

	void putImage();
	void setFullScreen(bool);
	void updateBackgroundColour(int colour, EmuTime &time);
	void updateBlanking(bool enabled, EmuTime &time);
	void updateDisplayMode(int mode, EmuTime &time);
	void updateNameBase(int addr, EmuTime &time);
	void updatePatternBase(int addr, int mask, EmuTime &time);
	void updateColourBase(int addr, int mask, EmuTime &time);
	void updateSpriteAttributeBase(int addr, EmuTime &time);
	void updateSpritePatternBase(int addr, EmuTime &time);
	void updateVRAM(int addr, byte data, EmuTime &time);

private:
	typedef void (SDLLoRenderer::*RenderMethod)(int line);
	typedef void (SDLLoRenderer::*PhaseHandler)(int limit);

	void renderUntil(int limit);

	void mode0(int line);
	void mode1(int line);
	void mode2(int line);
	void mode12(int line);
	void mode3(int line);
	void modebogus(int line);
	void mode23(int line);

	void offPhase(int limit);
	void blankPhase(int limit);
	void displayPhase(int limit);

	/** Draw sprites on this line over the background.
	  */
	void drawSprites(Pixel *linePtr, int line);

	/** Set all dirty / clean.
	  */
	void setDirty(bool);

	/** RenderMethods for each screen mode.
	  */
	static RenderMethod modeToRenderMethod[];
	/** The VDP of which the video output is being rendered.
	  */
	MSXTMS9928a *vdp;
	/** Display mode (M2..M0).
	  */
	int displayMode;
	/** SDL colours corresponding to each VDP palette entry.
	  * XPalFg has entry 0 set to the current background colour,
	  * XPalBg has entry 0 set to black.
	  */
	Pixel XPalFg[16], XPalBg[16];
	/** Phase handler: current drawing mode (off, blank, display).
	  */
	PhaseHandler currPhase;
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
	Pixel *cacheLinePtrs[192];
	/** Pointers to the start of each display line on the screen.
	  */
	Pixel *screenLinePtrs[192];
	/** Dirty tables indicate which character blocks must be repainted.
	  * The anyDirty variables are true when there is at least one
	  * element in the dirty table that is true.
	  */
	bool anyDirtyColour, dirtyColour[256 * 4];
	bool anyDirtyPattern, dirtyPattern[256 * 4];
	bool anyDirtyName, dirtyName[40 * 32];
	/** VRAM address where the name table starts.
	  */
	int nameBase;
	/** VRAM address where the pattern table starts.
	  */
	int patternBase;
	/** VRAM address where the colour table starts.
	  */
	int colourBase;
	/** VRAM address where the sprite attribute table starts.
	  */
	int spriteAttributeBase;
	/** VRAM address where the sprite pattern table starts.
	  */
	int spritePatternBase;
	/** Mask on the VRAM address in the pattern table.
	  */
	int patternMask;
	/** Mask on the VRAM address in the colour table.
	  */
	int colourMask;

};

#endif //__SDLLORENDERER_HH__

