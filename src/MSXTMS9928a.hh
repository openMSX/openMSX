// $Id$

#ifndef __MSXTMS9928A_HH__
#define __MSXTMS9928A_HH__

#include <SDL/SDL.h>

#include "openmsx.hh"
#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"
#include "HotKey.hh"


class Renderer;

class MSXTMS9928a : public MSXDevice, HotKeyListener
{
public:
	static const byte TMS9928A_PALETTE[];

	/** Constructor
	  */
	MSXTMS9928a(MSXConfig::Device *config);

	/** Destructor
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

	/** Handle "toggle fullscreen" hotkey requests.
	  */
	void signalHotKey(SDLKey key);

	/** Gets the current foreground colour [0..15].
	  */
	inline int getForegroundColour()
	{
		return controlRegs[7] >> 4;
	}
	/** Gets the current background colour [0..15].
	  */
	inline int getBackgroundColour()
	{
		return controlRegs[7] & 0x0F;
	}
	/** Gets the sprite size in pixels (8/16).
	  */
	inline int getSpriteSize()
	{
		return ((controlRegs[1] & 2) << 2) + 8;
	}
	/** Gets the sprite magnification (0 = normal, 1 = double).
	  */
	inline int getSpriteMag()
	{
		return controlRegs[1] & 1;
	}
	/** Is the display enabled? (false means blanked)
	  */
	inline bool displayEnabled()
	{
		return (controlRegs[1] & 0x40);
	}
	/** Are sprites enabled?
	  */
	inline bool spritesEnabled()
	{
		return !(controlRegs[1] & 0x10);
	}

	// Exposed for Renderer:
	// TODO: Reduce this to a minimum.
	// TODO: Is it possible to give just Renderer access to this?
	//       "Friend" declaration didn't work, but maybe something does.
	struct {
		/* TMS9928A internal settings */
		int Addr,FirstByte,BackColour,mode;
		int colour,pattern,nametbl,spriteattribute,spritepattern;
		int colourmask,patternmask;
		/* memory */
		byte *vMem;
		int vramsize, model;
	} tms;

	/** Check sprite collision and number of sprites per line.
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry int array in which
	  *   the numbers of the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  * TODO: Because this routine *must* be called by the renderer
	  *   for collision detection and 5th sprite detection to work,
	  *   sprite checking is still not 100% render independant.
	  */
	int checkSprites(int line, int *visibleSprites);

	/** Dirty tables indicate which character blocks must be repainted.
	  * The anyDirty variables are true when there is at least one
	  * element in the dirty table that is true.
	  */
	bool anyDirtyColour, dirtyColour[256 * 3];
	bool anyDirtyPattern, dirtyPattern[256 * 3];
	bool anyDirtyName, dirtyName[40 * 24];

private:
	/** Set all dirty / clean.
	  */
	void setDirty(bool);
	byte TMS9928A_vram_r();
	void _TMS9928A_change_register(byte reg, byte val);

	/** Renderer that converts this VDP's state into an image.
	  */
	Renderer *renderer;
	Emutime currentTime;
	/** Limit number of sprites per display line?
	  * Option only affects display, not MSX state.
	  * In other words: when false there is no limit to the number of
	  * sprites drawn, but the status register acts like the usual limit
	  * is still effective.
	  */
	bool limitSprites;
	/** VRAM or VDP regs changed since last frame?
	  * TODO: Adapt this for line-based emulation.
	  */
	bool stateChanged;
	/** Control registers.
	  */
	byte controlRegs[8];
	/** Status register.
	  */
	byte statusReg;
	/** VRAM is read as soon as VRAM pointer changes.
	  * TODO: Is this actually what happens?
	  *   On TMS9928 the VRAM interface is to only access method.
	  *   But on V9938/58 there are other ways to access VRAM;
	  *   I wonder if they are consistent with this implementation.
	  */
	byte readAhead;

};

#endif //___MSXTMS9928A_HH__

