// $Id$

#ifndef __DISPLAYMODE_HH__
#define __DISPLAYMODE_HH__

#include "openmsx.hh"

/** Represents a VDP display mode.
  * A display mode determines how bytes in the VRAM are converted to pixel
  * colours.
  * A display mode consists of a base mode with YJK filters on top.
  * Only the V9958 supports YJK filters.
  */
class DisplayMode
{
private:
	/** Display mode flags: YAE YJK M5..M1.
	  */
	int mode;

public:

	/** Bits of VDP register 0 that encode part of the display mode. */
	static const byte REG0_MASK = 0x0E;

	/** Bits of VDP register 1 that encode part of the display mode. */
	static const byte REG1_MASK = 0x18;
	
	/** Bits of VDP register 25 that encode part of the display mode. */
	static const byte REG25_MASK = 0x18;

	/** Encoding of base mode Text 1. */
	static const byte TEXT1 = 0x01;
	
	/** Encoding of base mode Text 2. */
	static const byte TEXT2 = 0x09;
	
	/** Encoding of base mode Multicolour. */
	static const byte MULTICOLOUR = 0x02;
	
	/** Encoding of base mode Graphic 1. */
	static const byte GRAPHIC1 = 0x00;
	
	/** Encoding of base mode Graphic 2. */
	static const byte GRAPHIC2 = 0x04;
	
	/** Encoding of base mode Graphic 3. */
	static const byte GRAPHIC3 = 0x08;
	
	/** Encoding of base mode Graphic 4. */
	static const byte GRAPHIC4 = 0x0C;
	
	/** Encoding of base mode Graphic 5. */
	static const byte GRAPHIC5 = 0x10;
	
	/** Encoding of base mode Graphic 6. */
	static const byte GRAPHIC6 = 0x14;
	
	/** Encoding of base mode Graphic 7. */
	static const byte GRAPHIC7 = 0x1C;
	
	/** Encoding of YJK flag. */
	static const byte YJK = 0x20;
	
	/** Encoding of YAE flag. */
	static const byte YAE = 0x40;
	
	/** Create the initial display mode.
	  */
	DisplayMode() {
		reset();
	}

	/** Create a specific display mode.
	  * @param reg0 The contents of VDP register 0.
	  * @param reg1 The contents of VDP register 1.
	  * @param reg25 The contents of VDP register 25;
	  * 	on non-V9958 chips, pass 0.
	  */
	DisplayMode(byte reg0, byte reg1, byte reg25) {
		mode = 
			  ((reg25 & 0x18) << 2) // YAE YJK
			| ((reg0  & 0x0E) << 1) // M5..M3
			| ((reg1  & 0x08) >> 2) // M2
			| ((reg1  & 0x10) >> 4) // M1
			;
	}

	/** Bring the display mode to its initial state.
	  */
	inline void reset() {
		mode = 0;
	}

	/** Assignment operator.
	  */
	inline DisplayMode &operator =(const DisplayMode &newMode) {
		mode = newMode.mode;
		return *this;
	}

	/** Equals operator.
	  */
	inline bool operator ==(const DisplayMode &otherMode) const {
		return mode == otherMode.mode;
	}
	
	/** Get the dispay mode as a byte: YAE YJK M5..M1 combined.
	  * @return The byte representation of this display mode,
	  * 	in the range [0..0x7F).
	  */
	inline byte getByte() const {
		return byte(mode);
	}

	/** Get the base dispay mode as an integer: M5..M1 combined.
	  * If YJK is active, the base mode is the underlying display mode.
	  * @return The integer representation of the base of this display mode,
	  * 	in the range [0..0x1F).
	  */
	inline int getBase() const {
		return mode & 0x1F;
	}

	/** Was this mode introduced by the V9938?
	  * @return True iff the base of this mode only is available on V9938/58.
	  */
	inline bool isV9938Mode() const {
		return (mode & 0x18) != 0;
	}

	/** Is the current mode a text mode?
	  * Text1 and Text2 are text modes.
	  * @return True iff the current mode is a text mode.
	  */
	inline bool isTextMode() const {
		// TODO: Is the display mode check OK? Profile undefined modes.
		return (mode & 0x17) == 0x01;
	}

	/** Is the current mode a bitmap mode?
	  * Graphic4 and higher are bitmap modes.
	  * @return True iff the current mode is a bitmap mode.
	  */
	inline bool isBitmapMode() const {
		return getBase() >= 0x0C;
	}

	/** Is VRAM "planar" in the current display mode?
	  * Graphic 6 and 7 spread their bytes over two VRAM ICs,
	  * such that the even bytes go to the first half of the address
	  * space and the odd bytes to the second half.
	  * @return True iff the current display mode has planar VRAM.
	  */
	inline bool isPlanar() const {
		// TODO: Is the display mode check OK? Profile undefined modes.
		return (mode & 0x14) == 0x14;
	}

	/** Get the sprite mode of this display mode.
	  * @return The current sprite mode:
	  * 	0 means no sprites,
	  * 	1 means sprite mode 1 (MSX1 display modes),
	  * 	2 means sprite mode 2 (MSX2 display modes).
	  */
	inline int getSpriteMode() const {
		return isTextMode() ? 0 : (isV9938Mode() ? 2 : 1);
	}

	/** Get number of pixels on a display line in this mode.
	  * @return 512 for Text 2, Graphic 5 and 6, 256 for all other modes.
	  * TODO: Would it make more sense to treat Text 2 as 480 pixels?
	  */
	inline int getLineWidth() const {
		byte baseMode = getBase();
		return
			( baseMode == TEXT2 || baseMode == GRAPHIC5 || baseMode == GRAPHIC6
			? 512
			: 256
			);
	}

};

#endif //__DISPLAYMODE_HH__

