// $Id$

#ifndef __MSXTMS9928A_HH__
#define __MSXTMS9928A_HH__

#include <SDL/SDL.h>

#include "openmsx.hh"
#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "EmuTime.hh"
#include "HotKey.hh"

class Renderer;


/** TMS9928a implementation: MSX1 Video Display Processor (VDP).
  * TODO: When communicating with the Renderer, when to use pull
  *   (getX on MSXTMS9928a) methods and when push (setX on Renderer)?
  *   Currently SDLLoRenderer implementation dictates this, but that
  *   is hardly design.
  *   One option would be to use all-pull for values and use update
  *   methods without parameters to send update events. However, even
  *   if getX is inline, parameter passing is probably faster still.
  */
class MSXTMS9928a : public MSXDevice, HotKeyListener
{
public:
	enum VdpVersion { TMS99X8, TMS99X8A, V9938, V9959 };
	typedef struct {
		int pattern;
		short int x;
		byte colour;
	} SpriteInfo;

	static const byte TMS9928A_PALETTE[];

	/** Constructor.
	  */
	MSXTMS9928a(MSXConfig::Device *config);

	/** Destructor.
	  */
	~MSXTMS9928a();

	// interaction with CPU
	byte readIO(byte port, EmuTime &time);
	void writeIO(byte port, byte value, EmuTime &time);
	void executeUntilEmuTime(const EmuTime &time);

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

	/** Read a byte from the VRAM.
	  * @param addr The address to read.
	  *   Do bounds checking is done, so make sure it is a legal address.
	  */
	inline byte getVRAM(int addr) {
		return vramData[addr];
	}
	/** Gets the current foreground colour [0..15].
	  */
	inline int getForegroundColour() {
		return controlRegs[7] >> 4;
	}
	/** Gets the current background colour [0..15].
	  */
	inline int getBackgroundColour() {
		return controlRegs[7] & 0x0F;
	}
	/** Is the display enabled? (false means blanked)
	  */
	inline bool displayEnabled() {
		return (controlRegs[1] & 0x40);
	}

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
	int checkSprites(int line, SpriteInfo *visibleSprites);

	/** Calculates a sprite pattern.
	  * @param patternNr Number of the sprite pattern [0..255].
	  *   For 16x16 sprites, patternNr should be a multiple of 4.
	  * @param y The line number within the sprite: 0 <= y < size.
	  * @return A bit field of the sprite pattern.
	  *   Bit 31 is the first bit of the sprite.
	  *   Unused bits are zero.
	  * TODO: Cache patterns? That's probably what the real VDP does.
	  *   Besides, that way pattern calculation is done only once.
	  */
	inline int calculatePattern(int patternNr, int y) {
		// TODO: Optimise getSpriteSize?
		if (getSpriteMag()) y /= 2;
		int pattern = spritePatternBasePtr[patternNr * 8 + y] << 24;
		if (getSpriteSize() == 16) {
			pattern |= spritePatternBasePtr[patternNr * 8 + y + 16] << 16;
		}
		if (getSpriteMag()) return doublePattern(pattern);
		else return pattern;
	}

private:
	/** Doubles a sprite pattern.
	  */
	static int doublePattern(int pattern);

	/** Gets the sprite size in pixels (8/16).
	  */
	inline int getSpriteSize() {
		return ((controlRegs[1] & 2) << 2) + 8;
	}
	/** Gets the sprite magnification (0 = normal, 1 = double).
	  */
	inline int getSpriteMag() {
		return controlRegs[1] & 1;
	}
	/** Are sprites enabled?
	  * @return True iff blanking is off and the current mode supports
	  *   sprites.
	  * TODO: For V9938, check bit 1 of reg 8 as well.
	  */
	inline bool spritesEnabled() {
		return (controlRegs[1] & 0x50) == 0x40;
	}
	/** Byte is read from VRAM by the CPU.
	  */
	byte vramRead();
	/** VDP control register has changed, work out the consequences.
	  */
	void changeRegister(byte reg, byte val, EmuTime &time);
	/** Display mode may have changed.
	  * If it has, update displayMode's value and inform the Renderer.
	  */
	void updateDisplayMode(EmuTime &time);

	/** Renderer that converts this VDP's state into an image.
	  */
	Renderer *renderer;
	/** Render full screen or windowed?
	  * @see Renderer::setFullScreen
	  * This setting is part of this class because it should remain
	  * consistent when switching renderers at run time.
	  */
	bool fullScreen;
	/** Limit number of sprites per display line?
	  * Option only affects display, not MSX state.
	  * In other words: when false there is no limit to the number of
	  * sprites drawn, but the status register acts like the usual limit
	  * is still effective.
	  */
	bool limitSprites;
	/** VDP version.
	  */
	VdpVersion version;
	/** Emulation time of the VDP.
	  * In other words, the time of the last update.
	  */
	EmuTime currentTime;
	/** Control registers.
	  */
	byte controlRegs[8];
	/** Status register.
	  */
	byte statusReg;
	/** Pointer to VRAM data block.
	  */
	byte *vramData;
	/** VRAM mask: bit mask that indicates which address bits are
	  * present in the VRAM.
	  * Equal to VRAM size minus one because VRAM size is a power of two.
	  */
	int vramMask;
	/** Pointer to VRAM at base address of sprite attribute table.
	  */
	byte *spriteAttributeBasePtr;
	/** Pointer to VRAM at base address of sprite pattern table.
	  */
	byte *spritePatternBasePtr;
	/** First byte written through port #99, or -1 for none.
	  */
	int firstByte;
	/** VRAM is read as soon as VRAM pointer changes.
	  * TODO: Is this actually what happens?
	  *   On TMS9928 the VRAM interface is to only access method.
	  *   But on V9938/58 there are other ways to access VRAM;
	  *   I wonder if they are consistent with this implementation.
	  */
	byte readAhead;
	/** Current dispay mode: M2..M0 combined.
	  */
	int displayMode;
	/** VRAM read/write access pointer.
	  * Not a C pointer, but an address.
	  */
	int vramPointer;

};

#endif //___MSXTMS9928A_HH__

