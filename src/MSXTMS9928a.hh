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

	// handle "toggle fullscreen"-hotkey requests
	void signalHotKey(SDLKey key);

	// Exposed for Renderer:
	// TODO: Reduce this to a minimum.
	// TODO: Is it possible to give just Renderer access to this?
	//       "Friend" declaration didn't work, but maybe something does.
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

	/** Limit number of sprites per display line?
	  * Option only affects display, not MSX state.
	  * In other words: when false there is no limit to the number of
	  * sprites drawn, but the status register acts like the usual limit
	  * is still effective.
	  */
	bool limitSprites;

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

private:
	Renderer *renderer;

	Emutime currentTime;

	byte TMS9928A_vram_r();

	void _TMS9928A_change_register(byte reg, byte val);
	/** Set all dirty / clean.
	  */
	void setDirty(bool);

};

#endif //___MSXTMS9928A_HH__

