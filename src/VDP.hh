// $Id$

#ifndef __VDP_HH__
#define __VDP_HH__

#include "openmsx.hh"
#include "MSXIODevice.hh"
#include "HotKey.hh"
#include "EmuTime.hh"
#include "Renderer.hh"

class VDPCmdEngine;

/** Unified implementation of MSX Video Display Processors (VDPs).
  * MSX1 VDP is Texas Instruments TMS9918A or TMS9928A.
  * MSX2 VDP is Yamaha V9938.
  * MSX2+ and turbo R VDP is Yamaha V9958.
  *
  * Current implementation is written by Maarten ter Huurne,
  * with some contributions from other openMSX developers.
  * The first implementation was based on Sean Young's code from MESS,
  * which was in turn based on code by Mike Balfour.
  * Integration of MESS code into openMSX was done by David Heremans.
  *
  * All undocumented features as described in the following file
  * should be emulated:
  * http://www.msxnet.org/tech/tms9918a.txt
  *
  * This class is the VDP core, it uses a separate Renderer to
  * convert the VRAM contents into pixels on the host machine.
  * Communicates with the Renderer through a pull variant of the
  * Observer pattern: a VDP object fires events when its state changes,
  * the Renderer can retrieve the new state if necessary by calling
  * methods on the VDP object.
  */
class VDP : public MSXIODevice, HotKeyListener
{
public:
	/** VDP version: the VDP model being emulated.
	  */
	enum VdpVersion {
		/** MSX1 VDP, NTSC version.
		  * TMS9918A has NTSC encoding built in,
		  * while TMS9928A has colour difference output;
		  * in emulation there is no difference.
		  */
		TMS99X8A,
		/** MSX1 VDP, PAL version.
		  */
		TMS9929A,
		/** MSX2 VDP.
		  */
		V9938,
		/** MSX2+ and turbo R VDP.
		  */
		V9958
	};

	/** Contains all the information to draw a line of a sprite.
	  */
	typedef struct {
		int pattern;
		short int x;
		byte colour;
	} SpriteInfo;

	/** Constructor.
	  */
	VDP(MSXConfig::Device *config, const EmuTime &time);

	/** Destructor.
	  */
	~VDP();

	// mainlife cycle of an MSXDevice
	void reset(const EmuTime &time);

	// interaction with CPU
	byte readIO(byte port, const EmuTime &time);
	void writeIO(byte port, byte value, const EmuTime &time);
	void executeUntilEmuTime(const EmuTime &time);

	// void saveState(ofstream writestream);
	// void restoreState(char *devicestring,ifstream readstream);

	/** Handle "toggle fullscreen" hotkey requests.
	  */
	void signalHotKey(SDLKey key);

	/** Is this an MSX1 VDP?
	  * @return True if this is an MSX1 VDP (TMS99X8A or TMS9929A),
	  *   False otherwise.
	  */
	inline bool isMSX1VDP() {
		return version == TMS99X8A || version == TMS9929A;
	}

	/** Get dispay mode: M5..M1 combined.
	  * @return The current display mode.
	  */
	inline int getDisplayMode() {
		return displayMode;
	}

	/** Read a byte from the VRAM.
	  * TODO: If called from the Renderer, sync with command engine first.
	  * @param addr The address to read.
	  *   No bounds checking is done, so make sure it is a legal address.
	  * @return The VRAM contents at the specified address.
	  */
	inline byte getVRAM(int addr) {
		//fprintf(stderr, "read VRAM @ %04X\n", addr);
		return vramData[addr];
	}

	/** Write a byte to the VRAM.
	  * @param addr The address to write.
	  *   No bounds checking is done, so make sure it is a legal address.
	  * @param value The value to write.
	  * @param time The moment in emulated time this write occurs.
	  * @return The VRAM contents at the specified address.
	  */
	inline void setVRAM(int addr, byte value, const EmuTime &time) {
		//fprintf(stderr, "write VRAM @ %04X\n", addr);
		// TODO: Remove the assert once I trust no bugs were introduced
		//       by the command engine integration.
		assert((addr & vramMask) == addr);
		if (vramData[addr] != value) {
			renderer->updateVRAM(addr, value, time);
			vramData[addr] = value;
		}
	}

	/** Gets the current transparency setting.
	  * @return True iff colour 0 is transparent.
	  */
	inline bool getTransparency() {
		return (controlRegs[8] & 0x20) == 0;
	}

	/** Gets the current foreground colour.
	  * @return Colour value [0..15].
	  */
	inline int getForegroundColour() {
		return controlRegs[7] >> 4;
	}

	/** Gets the current background colour.
	  * @return Colour value [0..15].
	  */
	inline int getBackgroundColour() {
		return controlRegs[7] & 0x0F;
	}

	/** Gets the current blinking colour for blinking text.
	  * @return Colour value [0..15].
	  */
	inline int getBlinkForegroundColour() {
		return controlRegs[12] >> 4;
	}

	/** Gets the current blinking colour for blinking text.
	  * @return Colour value [0..15].
	  */
	inline int getBlinkBackgroundColour() {
		return controlRegs[12] & 0x0F;
	}

	/** Gets the current blink state.
	  * @return True iff alternate colours / page should be displayed.
	  */
	inline bool getBlinkState() {
		return blinkState;
	}

	/** Gets a palette entry.
	  * @param index The index [0..15] in the palette.
	  * @return Colour value in the format of the palette registers:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  */
	inline int getPalette(int index) {
		return palette[index];
	}

	/** Is the display enabled?
	  * @return True if enabled, False if blanked.
	  */
	inline bool isDisplayEnabled() {
		return (controlRegs[1] & 0x40);
	}

	/** Get name table mask.
	  * When this mask is applied to a table index with leading ones,
	  * the result is the VRAM address of that index in the name table.
	  * @return Current pattern name table mask.
	  */
	inline int getNameMask() {
		return nameBase;
	}

	/** Get pattern table mask.
	  * When this mask is applied to a table index with leading ones,
	  * the result is the VRAM address of that index in the pattern table.
	  * @return Current pattern generator table mask.
	  */
	inline int getPatternMask() {
		return patternBase;
	}

	/** Get colour table mask.
	  * When this mask is applied to a table index with leading ones,
	  * the result is the VRAM address of that index in the colour table.
	  * @return Current colour table base.
	  */
	inline int getColourMask() {
		return colourBase;
	}

	/** Get sprite attribute table base.
	  * TODO: Why is this not a mask?
	  * @return Current sprite attribute table base.
	  */
	inline int getSpriteAttributeBase() {
		return spriteAttributeBase;
	}

	/** Get sprite pattern table base.
	  * TODO: Why is this not a mask?
	  * @return Current sprite pattern table base.
	  */
	inline int getSpritePatternBase() {
		return spritePatternBase;
	}

	/** Get sprites for a display line.
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be returned.
	  * @param visibleSprites Output parameter in which the pointer to
	  *   a SpriteInfo array containing the sprites to be displayed is
	  *   returned.
	  *   The array's contents are valid until the next time the VDP
	  *   is scheduled.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int getSprites(int line, SpriteInfo *&visibleSprites) {
		visibleSprites = spriteBuffer[line];
		return spriteCount[line];
	}

	/** Gets the current vertical scroll (line displayed at Y=0).
	  * @return Vertical scroll register value.
	  */
	inline byte getVerticalScroll() {
		return controlRegs[23];
	}

	/** Gets the number of lines per screen.
	  * @return 192 or 212.
	  */
	inline int getNumberOfLines() {
		return (controlRegs[9] & 0x80 ? 212 : 192);
	}

	/** Get VRAM access timing info.
	  * This is the internal format used by the command engine.
	  * TODO: When improving the timing accuracy, think of a clearer
	  *       way of sharing this information.
	  */
	inline int getAccessTiming() {
		return ((controlRegs[1]>>6) & 1)  // display enable
			| (controlRegs[8] & 2)        // sprite enable
			| ((controlRegs[9]<<1) & 4);  // 192/212 lines
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
	  * @return True iff blanking is off, the current mode supports
	  *   sprites and sprites are not disabled.
	  */
	inline bool spritesEnabled() {
		return ((controlRegs[1] & 0x50) == 0x40)
			&& ((controlRegs[8] & 0x02) == 0x00);
	}

	/** Calculates a sprite pattern.
	  * @param patternNr Number of the sprite pattern [0..255].
	  *   For 16x16 sprites, patternNr should be a multiple of 4.
	  * @param y The line number within the sprite: 0 <= y < size.
	  * @return A bit field of the sprite pattern.
	  *   Bit 31 is the leftmost bit of the sprite.
	  *   Unused bits are zero.
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

	/** Called both on init and on reset.
	  * Puts VDP into reset state.
	  * Does not call any renderer methods.
	  */
	void resetInit(const EmuTime &time);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 1 (MSX1).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry SpriteInfo array
	  *   in which the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	int checkSprites1(int line, SpriteInfo *visibleSprites);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 2 (MSX2).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry SpriteInfo array
	  *   in which the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	int checkSprites2(int line, SpriteInfo *visibleSprites);

	/** Byte is read from VRAM by the CPU.
	  */
	byte vramRead();

	/** VDP control register has changed, work out the consequences.
	  */
	void changeRegister(byte reg, byte val, const EmuTime &time);

	/** Display mode may have changed.
	  * If it has, update displayMode's value and inform the Renderer.
	  */
	void updateDisplayMode(byte reg0, byte reg1, const EmuTime &time);

	/** Renderer that converts this VDP's state into an image.
	  */
	Renderer *renderer;

	/** Command engine: the part of the V9938/58 that executes commands.
	  */
	VDPCmdEngine *cmdEngine;

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
	EmuTimeFreq<3579545> currentTime;

	/** Control registers.
	  */
	byte controlRegs[32];

	/** Mask on the control register index:
	  * makes MSX2 registers inaccessible on MSX1.
	  */
	int controlRegMask;

	/** Mask on the values of control registers.
	  */
	byte controlValueMasks[32];

	/** Status register 0.
	  */
	byte statusReg0;

	/** Status register 1.
	  * TODO: Only bit 0 can change, use a bool instead of a byte?
	  */
	byte statusReg1;

	/** Status register 2.
	  * Bit 7, 4 and 0 of this field are always zero,
	  * their value can be retrieved from the command engine.
	  */
	byte statusReg2;

	/** X coordinate of sprite collision.
	  * 8 bits long -> [0..511]?
	  */
	int collisionX;

	/** Y coordinate of sprite collision.
	  * 8 bits long -> [0..511]?
	  * Bit 9 contains EO, I guess that's a copy of the even/odd flag
	  * of the frame on which the collision occurred.
	  */
	int collisionY;

	/** V9938 palette.
	  */
	word palette[16];

	/** Blinking state: should alternate colour / page be displayed?
	  * TODO: Implement toggling when blinking is enabled.
	  */
	bool blinkState;

	/** First byte written through port #9A, or -1 for none.
	  */
	int paletteLatch;

	/** Pointer to VRAM data block.
	  */
	byte *vramData;

	/** VRAM mask: bit mask that indicates which address bits are
	  * present in the VRAM.
	  * Equal to VRAM size minus one because VRAM size is a power of two.
	  */
	int vramMask;

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

	/** Pointer to VRAM at base address of sprite attribute table.
	  * TODO: Keep this? Is it secure (out-of-bounds)?
	  */
	byte *spriteAttributeBasePtr;

	/** Pointer to VRAM at base address of sprite pattern table.
	  * TODO: Keep this? Is it secure (out-of-bounds)?
	  */
	byte *spritePatternBasePtr;

	/** Buffer containing the sprites that are visible on each
	  * display line.
	  */
	SpriteInfo spriteBuffer[212][32];

	/** Buffer containing the number of sprites that are visible
	  * on each display line.
	  * In other words, spriteCount[i] is the number of sprites
	  * in spriteBuffer[i].
	  */
	int spriteCount[212];

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

	/** Current dispay mode: M5..M1 combined.
	  */
	int displayMode;

	/** VRAM read/write access pointer.
	  * Contains the lower 14 bits of the current VRAM access address.
	  */
	int vramPointer;

};

#endif //__VDP_HH__

