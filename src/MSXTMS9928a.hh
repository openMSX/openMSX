// $Id$

#ifndef __MSXTMS9928A_HH__
#define __MSXTMS9928A_HH__

#include <SDL/SDL.h>

#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"
#include "HotKey.hh"


#define WIDTH 320
#define HEIGHT 240
#define DEPTH 8

#if DEPTH == 8
typedef byte Pixel;
#elif DEPTH == 15 || DEPTH == 16
typedef word Pixel;
#elif DEPTH == 32
typedef unsigned int Pixel;
#else
#error DEPTH must be 8, 15, 16 or 32 bits per pixel.
#endif

class MSXTMS9928a;



// TODO: Introduce an abstract base class to SDLRenderer.
class SDLRenderer
{
public:
	typedef void (SDLRenderer::*RenderMethod)(Pixel *pixelPtr, int line);
	static RenderMethod modeToRenderMethod[];

	SDLRenderer(MSXTMS9928a *vdp, bool fullScreen);
	//~SDLRenderer();
	void toggleFullScreen();
	void mode0(Pixel *pixelPtr, int line);
	void mode1(Pixel *pixelPtr, int line);
	void mode2(Pixel *pixelPtr, int line);
	void mode12(Pixel *pixelPtr, int line);
	void mode3(Pixel *pixelPtr, int line);
	void modebogus(Pixel *pixelPtr, int line);
	void mode23(Pixel *pixelPtr, int line);
	void modeblank(Pixel *pixelPtr, int line);

	/** Draw sprites on this line over the background.
	* @param dirty 32-entry array that stores which characters are
	*   covered with sprites and must therefore be redrawn next frame.
	*   This method will update the array according to the sprites drawn.
	* @return Were any pixels drawn?
	*/
	bool drawSprites(Pixel *pixelPtr, int line, bool *dirty);

	/** Put an image on the screen.
	*/
	void putImage();

	void fullScreenRefresh();

private:
	MSXTMS9928a *vdp;

	Pixel XPal[16];
	Pixel currBorderColours[HEIGHT];

	SDL_Surface *screen;

	/** Actual pixel data.
	*/
	Pixel pixelData[WIDTH * HEIGHT];
	/** Pointers to the start of each line.
	*/
	Pixel *linePtrs[HEIGHT];
};



class MSXTMS9928a : public MSXDevice, HotKeyListener
{
	// TODO: Move to black-box reuse if possible.
	//   If this does not combine with good performance,
	//   select a minimal number of fields to be accessible
	//   from the renderer class.
	friend class SDLRenderer;
public:
	/**
	 * Constructor
	 */
	MSXTMS9928a(MSXConfig::Device *config);

	/**
	 * Destructor
	 */
	~MSXTMS9928a();

	// interaction with CPU
	byte readIO(byte port, Emutime &time);
	void writeIO(byte port, byte value, Emutime &time);
	void executeUntilEmuTime(const Emutime &time);

	// mainlife cycle of an MSXDevice
	void init();
	void start();
	//void stop();
	void reset();

	// void saveState(ofstream writestream);
	// void restoreState(char *devicestring,ifstream readstream);

	// handle "toggle fullscreen"-hotkey requests
	void signalHotKey(SDLKey key);
private:
	SDLRenderer *renderer;

	Emutime currentTime;

	/** Limit number of sprites per display line?
	  * Option only affects display, not MSX state.
	  * In other words: when false there is no limit to the number of
	  * sprites drawn, but the status register acts like the usual limit
	  * is still effective.
	  */
	bool limitSprites;

	struct {
		/* TMS9928A internal settings */
		byte ReadAhead,Regs[8],StatusReg;
		int Addr,FirstByte,BackColour,Change,mode;
		int colour,pattern,nametbl,spriteattribute,spritepattern;
		int colourmask,patternmask;
		/* memory */
		byte *vMem;
		int vramsize, model;
	} tms;

	byte TMS9928A_vram_r();

	void _TMS9928A_change_register(byte reg, byte val);
	/** Set all dirty / clean.
	  */
	void setDirty(bool);

	/** Check sprite collision and number of sprites per line.
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry int array in which
	  *   the numbers of the sprites to be displayed are returned.
	  * @param size Sprite size: 8 or 16.
	  * @param mag Magnification: 0 (normal) or 1 (doubled).
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	int checkSprites(int line, int *visibleSprites, int size, int mag);

	/* emulation settings */
	/* all or nothing dirty David Heremans */
	bool stateChanged;
	/* dirty tables from Sean Young restcode */
	bool anyDirtyColour, dirtyColour[256 * 3];
	bool anyDirtyPattern, dirtyPattern[256 * 3];
	bool anyDirtyName, dirtyName[40 * 24];

};

#endif //___MSXTMS9928A_HH__

