// $Id$

#ifndef __VDP_HH__
#define __VDP_HH__

#include "openmsx.hh"
#include "Scheduler.hh"
#include "MSXIODevice.hh"
#include "MSXMotherBoard.hh"
#include "EmuTime.hh"
#include "Renderer.hh"
#include "ConsoleSource/ConsoleCommand.hh"


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
  *
  * A note about timing: the start of a frame or line is defined as
  * the starting time of the corresponding sync (vsync, hsync).
  */
class VDP : public MSXIODevice, private Schedulable
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

	typedef unsigned int SpritePattern;

	/** Contains all the information to draw a line of a sprite.
	  */
	typedef struct {
		/** Bit mask of length 32: 1 is visible, 0 is transparent.
		  */
		SpritePattern pattern;
		/** X-coordinate of sprite, corrected for early clock.
		  */
		short int x;
		/** Bit 3..0 are index in palette.
		  * Bit 6 is 0 for sprite mode 1 like behaviour,
		  * or 1 for OR-ing of sprite colours.
		  * Other bits are undefined.
		  */
		byte colourAttrib;
	} SpriteInfo;

	/** Number of VDP clock ticks per line.
	  */
	static const int TICKS_PER_LINE = 1368;

	/** Constructor.
	  */
	VDP(MSXConfig::Device *config, const EmuTime &time);

	/** Destructor.
	  */
	~VDP();

	// mainlife cycle of an MSXDevice
	void reset(const EmuTime &time);

	// TODO: This shouldn't be necessary, according to comments
	//       in MSXDevice, but without it, VDP is called "no name".
	virtual const std::string &getName() {
		static std::string name("VDP");
		return name;
	}

	// interaction with CPU
	byte readIO(byte port, const EmuTime &time);
	void writeIO(byte port, byte value, const EmuTime &time);
	void executeUntilEmuTime(const EmuTime &time, int userData);

	// void saveState(ofstream writestream);
	// void restoreState(char *devicestring,ifstream readstream);

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

	/** Is the current mode a text mode?
	  * Text1 and Text2 are text modes.
	  * @return True iff the current mode is a bitmap mode.
	  */
	inline bool isTextMode() {
		// TODO: Is the display mode check OK? Profile undefined modes.
		return (displayMode & 0x17) == 0x01;
	}

	/** Is the current mode a bitmap mode?
	  * Graphic4 and higher are bitmap modes.
	  * @return True iff the current mode is a bitmap mode.
	  */
	inline bool isBitmapMode() {
		return (displayMode & 0x10) || ((displayMode & 0x0C) == 0x0C);
	}

	/** Is VRAM "planar" in the current display mode?
	  * Graphic6 and 7 spread their bytes over two VRAM ICs,
	  * such that the even bytes go to the first half of the address
	  * space and the odd bytes to the second half.
	  * @return True iff the current display mode has planar VRAM.
	  */
	inline bool isPlanar() {
		// TODO: Is the display mode check OK? Profile undefined modes.
		return (displayMode & 0x14) == 0x14;
	}

	/** Read a byte from the VRAM.
	  * Ignores planar addressing: since renderer and command engine
	  * know the display mode there is no need to dynamically handle
	  * planar/non-planar addressing.
	  * TODO: If called from the Renderer, sync with command engine first.
	  * @param addr The address to read.
	  *   No bounds checking is done, so make sure it is a legal address.
	  * @return The VRAM contents at the specified address.
	  */
	inline byte getVRAM(int addr) {
		//fprintf(stderr, "read VRAM @ %04X\n", addr);
		return vramData[addr];
	}

	/** Read a byte from the VRAM.
	  * Takes planar addressing into account if necessary.
	  * @param addr The address to read.
	  *   No bounds checking is done, so make sure it is a legal address.
	  * @return The VRAM contents at the specified address.
	  */
	inline byte getVRAMReordered(int addr) {
		return getVRAM(
			isPlanar() ? ((addr << 16) | (addr >> 1)) & vramMask : addr);
	}

	/** Write a byte to the VRAM.
	  * Ignores planar addressing: since renderer and command engine
	  * know the display mode there is no need to dynamically handle
	  * planar/non-planar addressing.
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

	/** Write a byte to the VRAM.
	  * Takes planar addressing into account if necessary.
	  * @param addr The address to write.
	  *   No bounds checking is done, so make sure it is a legal address.
	  * @param value The value to write.
	  * @param time The moment in emulated time this write occurs.
	  * @return The VRAM contents at the specified address.
	  */
	inline void setVRAMReordered(int addr, byte value, const EmuTime &time) {
		setVRAM(
			isPlanar() ? ((addr << 16) | (addr >> 1)) & vramMask : addr,
			value, time);
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
		if (line >= spriteLine) {
			/*
			cout << "performing extra updateSprites: "
				<< "old line = " << (spriteLine - 1)
				<< ", new line = " << line
				<< "\n";
			*/

			// TODO: Duplicated code.
			// TODO: Would be faster if we knew how much to render ahead.
			//       Either VDP keeps track or renderer gives parameter.
			if (displayMode < 8) {
				updateSprites1(line + 1);
			} else {
				updateSprites2(line + 1);
			}
		}
		visibleSprites = spriteBuffer[line];
		return spriteCount[line];
	}

	/** Gets the current vertical scroll (line displayed at Y=0).
	  * @return Vertical scroll register value.
	  */
	inline byte getVerticalScroll() {
		return controlRegs[23];
	}

	/** Gets the current horizontal display adjust.
	  * @return Adjust: 0 is leftmost, 7 is center, 15 is rightmost.
	  */
	inline int getHorizontalAdjust() {
		return (controlRegs[18] & 0x0F) ^ 0x07;
	}

	/** Gets the current vertical display adjust.
	  * @return Adjust: 0 is topmost, 7 is center, 15 is bottommost.
	  */
	inline int getVerticalAdjust() {
		return verticalAdjust;
	}

	/** Gets the number of display lines per screen.
	  * @return 192 or 212.
	  */
	inline int getNumberOfLines() {
		return (controlRegs[9] & 0x80 ? 212 : 192);
	}

	/** Get VDP ticks between start of frame and start of display.
	  */
	inline int getDisplayStart() {
		return displayStart;
	}

	/** Is PAL timing active?
	  * @return True if PAL timing, false if NTSC timing.
	  */
	inline bool isPalTiming() {
		return palTiming;
	}

	/** Gets the number of VDP clock ticks (21MHz) elapsed between
	  * a given time and the start of this frame.
	  */
	inline int getTicksThisFrame(const EmuTime &time) {
		return frameStartTime.getTicksTill(time);
	}

	/** Get VRAM access timing info.
	  * This is the internal format used by the command engine.
	  * TODO: When improving the timing accuracy, think of a clearer
	  *       way of sharing this information.
	  */
	inline int getAccessTiming() {
		return ((controlRegs[1]>>6) & 1)  // display enable
			| (controlRegs[8] & 2)        // sprite enable
			| (palTiming ? 4 : 0);        // NTSC/PAL
	}

private:
	class PaletteCmd : public ConsoleCommand {
	public:
		PaletteCmd(VDP *vdp);
		virtual void execute(const char *commandLine);
		virtual void help(const char *commandLine);
	private:
		VDP *vdp;
	};

	/** Types of VDP sync points that can be scheduled.
	  */
	enum SyncType {
		/** Vertical sync: the transition from one frame to the next.
		  */
		VSYNC,
		/** Vertical scanning: end of display.
		  */
		VSCAN,
		/** Horizontal scanning: line interrupt.
		  */
		HSCAN
	};

	/** Doubles a sprite pattern.
	  */
	static SpritePattern doublePattern(SpritePattern pattern);

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

	/** Calculate sprite patterns.
	  */
	inline void updateSprites(const EmuTime &time) {
		int limit =
			(getTicksThisFrame(time) - displayStart) / TICKS_PER_LINE;
		// TODO: Use method pointer.
		if (displayMode < 8) {
			updateSprites1(limit);
		} else {
			updateSprites2(limit);
		}
	}

	/** Calculates a sprite pattern.
	  * @param patternNr Number of the sprite pattern [0..255].
	  *   For 16x16 sprites, patternNr should be a multiple of 4.
	  * @param y The line number within the sprite: 0 <= y < size.
	  * @return A bit field of the sprite pattern.
	  *   Bit 31 is the leftmost bit of the sprite.
	  *   Unused bits are zero.
	  */
	inline SpritePattern calculatePattern(int patternNr, int y);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 1 (MSX1).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry SpriteInfo array
	  *   in which the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int checkSprites1(int line, SpriteInfo *visibleSprites);

	/** Check sprite collision and number of sprites per line.
	  * This routine implements sprite mode 2 (MSX2).
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param line The line number for which sprites should be checked.
	  * @param visibleSprites Pointer to a 32-entry SpriteInfo array
	  *   in which the sprites to be displayed are returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  */
	inline int checkSprites2(int line, SpriteInfo *visibleSprites);

	/** Gets the number of VDP clockticks (21MHz) per frame.
	  */
	inline int getTicksPerFrame() {
		return (palTiming ? TICKS_PER_LINE * 313 : TICKS_PER_LINE * 262);
	}

	/** Gets the value of the horizontal retrace status bit.
	  * Note that HR flipping continues at all times, not just during
	  * vertical display range.
	  * TODO: This method is used just once currently, if it stays
	  *   that way substitute the code instead of having a method.
	  * @param ticksThisFrame The screen position (in VDP ticks)
	  *    to return HR for.
	  * @return True iff the VDP scanning is inside the left/right
	  *   border or left/right erase or horizontal sync.
	  *   False iff the VDP scanning is in the display range.
	  */
	inline bool getHR(int ticksThisFrame) {
		// TODO: Use display adjust register (R#18).
		return (isTextMode()
			? (ticksThisFrame + (87 + 27)) % TICKS_PER_LINE
			  < (TICKS_PER_LINE - 960)
			: (ticksThisFrame + (59 + 27)) % TICKS_PER_LINE
			  < (TICKS_PER_LINE - 1024)
			);
	}

	/** Called both on init and on reset.
	  * Puts VDP into reset state.
	  * Does not call any renderer methods.
	  */
	void resetInit(const EmuTime &time);

	/** Start a new frame.
	  * @param time The moment in emulated time the frame starts.
	  */
	void frameStart(const EmuTime &time);

	/** Schedules a HSCAN sync point.
	  * Also removes a pending HSCAN sync, if any.
	  * @param time The moment in emulated time this call takes place.
	  *   Note: time is not the HSCAN sync time!
	  */
	void scheduleHScan(const EmuTime &time);

	/** Calculate sprite pattern for sprite mode 1.
	  */
	void updateSprites1(int limit);

	/** Calculate sprite pattern for sprite mode 2.
	  */
	void updateSprites2(int limit);

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

	/** The emulation time when this frame was started (vsync).
	  */
	EmuTimeFreq<21477270> frameStartTime;

	/** Manages vertical scanning interrupt request.
	  */
	MSXMotherBoard::IRQHelper irqVertical;

	/** Manages horizontal scanning interrupt request.
	  */
	MSXMotherBoard::IRQHelper irqHorizontal;

	/** VDP ticks between start of frame and start of display.
	  */
	int displayStart;

	/** VDP ticks between start of frame and the moment horizontal
	  * scan match occurs.
	  */
	int horizontalScanOffset;

	/** Time of last set HSCAN sync point.
	  */
	EmuTime hScanSyncTime;

	/** Is PAL timing active? False means NTSC timing.
	  * This value is updated at the start of every frame,
	  * to avoid problems with mid-screen PAL/NTSC switching.
	  * @see isPalTiming.
	  */
	bool palTiming;

	/** Vertical display adjust.
	  * This value is updated at the start of every frame.
	  * @see getVerticalAdjust.
	  */
	int verticalAdjust;

	/** Sprites are checked up to and excluding this display line.
	  */
	int spriteLine;

	/** Control registers.
	  */
	byte controlRegs[32];

	/** Mask on the control register index:
	  * makes MSX2 registers inaccessible on MSX1,
	  * instead the MSX1 registers are mirrored.
	  */
	int controlRegMask;

	/** Mask on the values of control registers.
	  */
	byte controlValueMasks[32];

	/** Status register 0.
	  */
	byte statusReg0;

	/** Status register 1.
	  * Bit 7 and 6 are always zero because light pen is not implemented.
	  * Bit 0 is always zero; its calculation depends on IE1.
	  * So all that remains is the version number.
	  */
	byte statusReg1;

	/** Status register 2.
	  * Bit 7, 4 and 0 of this field are always zero,
	  * their value can be retrieved from the command engine.
	  */
	byte statusReg2;

	/** X coordinate of sprite collision.
	  * 9 bits long -> [0..511]?
	  */
	int collisionX;

	/** Y coordinate of sprite collision.
	  * 9 bits long -> [0..511]?
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
	  *   On TMS9928 the VRAM interface is the only access method.
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

	/** Implements the palette print command.
	  */
	PaletteCmd paletteCmd;

};

#endif //__VDP_HH__

