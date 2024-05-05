#ifndef DISPLAYMODE_HH
#define DISPLAYMODE_HH

#include "openmsx.hh"
#include "one_of.hh"

namespace openmsx {

/** Represents a VDP display mode.
  * A display mode determines how bytes in the VRAM are converted to pixel
  * colors.
  * A display mode consists of a base mode with YJK filters on top.
  * Only the V9958 supports YJK filters.
  */
class DisplayMode
{
private:
	constexpr explicit DisplayMode(byte mode_) : mode(mode_) {}
	/** Display mode flags: YAE YJK M5..M1.
	  * The YAE flag indicates whether YAE is active, not just the value of
	  * the corresponding mode bit; so if YJK is 0, YAE is 0 as well.
	  */
	byte mode = 0;

public:
	static constexpr uint8_t GRAPHIC1   = 0x00; // Graphic 1
	static constexpr uint8_t TEXT1      = 0x01; // Text 1
	static constexpr uint8_t MULTICOLOR = 0x02; // Multicolor
	static constexpr uint8_t GRAPHIC2   = 0x04; // Graphic 2
	static constexpr uint8_t TEXT1Q     = 0x05; // !!
	static constexpr uint8_t MULTIQ     = 0x06; // !!
	static constexpr uint8_t GRAPHIC3   = 0x08; // Graphic 3
	static constexpr uint8_t TEXT2      = 0x09; // Text 2
	static constexpr uint8_t GRAPHIC4   = 0x0C; // Graphic 4
	static constexpr uint8_t GRAPHIC5   = 0x10; // Graphic 5
	static constexpr uint8_t GRAPHIC6   = 0x14; // Graphic 6
	static constexpr uint8_t GRAPHIC7   = 0x1C; // Graphic 7

	/** Bits of VDP register 0 that encode part of the display mode. */
	static constexpr byte REG0_MASK = 0x0E;

	/** Bits of VDP register 1 that encode part of the display mode. */
	static constexpr byte REG1_MASK = 0x18;

	/** Bits of VDP register 25 that encode part of the display mode. */
	static constexpr byte REG25_MASK = 0x18;

	/** Encoding of YJK flag. */
	static constexpr byte YJK = 0x20;

	/** Encoding of YAE flag. */
	static constexpr byte YAE = 0x40;

	/** Create the initial display mode.
	  */
	constexpr DisplayMode() = default;

	/** Create a specific display mode.
	  * @param reg0 The contents of VDP register 0.
	  * @param reg1 The contents of VDP register 1.
	  * @param reg25 The contents of VDP register 25;
	  *     on non-V9958 chips, pass 0.
	  */
	constexpr DisplayMode(byte reg0, byte reg1, byte reg25) {
		if ((reg25 & 0x08) == 0) reg25 = 0; // If YJK is off, ignore YAE.
		mode = byte(
			((reg25 & 0x18) << 2) | // YAE YJK
			((reg0  & 0x0E) << 1) | // M5..M3
			((reg1  & 0x08) >> 2) | // M2
			((reg1  & 0x10) >> 4)); // M1
	}

	[[nodiscard]] constexpr DisplayMode updateReg25(byte reg25) const {
		if ((reg25 & 0x08) == 0) reg25 = 0; // If YJK is off, ignore YAE.
		return DisplayMode(byte(getBase() | ((reg25 & 0x18) << 2)));
	}

	/** Bring the display mode to its initial state.
	  */
	constexpr void reset() {
		*this = DisplayMode();
	}

	/** Equals operator.
	  */
	[[nodiscard]] constexpr bool operator==(const DisplayMode&) const = default;

	/** Get the display mode as a byte: YAE YJK M5..M1 combined.
	  * @return The byte representation of this display mode,
	  *     in the range [0..0x7F].
	  */
	[[nodiscard]] constexpr byte getByte() const {
		return mode;
	}

	/** Used for de-serialization. */
	constexpr void setByte(byte mode_) {
		mode = mode_;
	}

	/** Get the base display mode as an integer: M5..M1 combined.
	  * If YJK is active, the base mode is the underlying display mode.
	  * @return The integer representation of the base of this display mode,
	  *     in the range [0..0x1F].
	  */
	[[nodiscard]] constexpr byte getBase() const {
		return mode & 0x1F;
	}

	/** Was this mode introduced by the V9938?
	  * @return True iff the base of this mode only is available on V9938/58.
	  */
	[[nodiscard]] constexpr bool isV9938Mode() const {
		return (mode & 0x18) != 0;
	}

	/** Is the current mode a text mode?
	  * Text1 and Text2 are text modes.
	  * @return True iff the current mode is a text mode.
	  */
	[[nodiscard]] constexpr bool isTextMode() const {
		return getBase() == one_of(TEXT1, TEXT2, TEXT1Q);
	}

	/** Is the current mode a bitmap mode?
	  * Graphic4 and higher are bitmap modes.
	  * @return True iff the current mode is a bitmap mode.
	  */
	[[nodiscard]] constexpr bool isBitmapMode() const {
		return getBase() >= 0x0C;
	}

	/** Is VRAM "planar" in the current display mode?
	  * Graphic 6 and 7 spread their bytes over two VRAM ICs,
	  * such that the even bytes go to the first half of the address
	  * space and the odd bytes to the second half.
	  * @return True iff the current display mode has planar VRAM.
	  */
	[[nodiscard]] constexpr bool isPlanar() const {
		// TODO: Is the display mode check OK? Profile undefined modes.
		return (mode & 0x14) == 0x14;
	}

	/** Are sprite pixels narrow?
	  */
	[[nodiscard]] constexpr bool isSpriteNarrow() const {
		// TODO: Check what happens to sprites in Graphic5 + YJK/YAE.
		return mode == GRAPHIC5;
	}

	/** Get the sprite mode of this display mode.
	  * @return The current sprite mode:
	  *     0 means no sprites,
	  *     1 means sprite mode 1 (MSX1 display modes),
	  *     2 means sprite mode 2 (MSX2 display modes).
	  */
	[[nodiscard]] constexpr int getSpriteMode(bool isMSX1) const {
		switch (getBase()) {
		case GRAPHIC1: case MULTICOLOR: case GRAPHIC2:
			return 1;
		case MULTIQ: // depends on VDP type
			return isMSX1 ? 1 : 0;
		case GRAPHIC3: case GRAPHIC4: case GRAPHIC5:
		case GRAPHIC6: case GRAPHIC7:
			return 2;
		case TEXT1: case TEXT1Q: case TEXT2:
		default: // and all other (bogus) modes
			// Verified on real V9958: none of the bogus modes
			// show sprites.
			// TODO check on TMSxxxx
			return 0;
		}
	}

	/** Get number of pixels on a display line in this mode.
	  * @return 512 for Text 2, Graphic 5 and 6, 256 for all other modes.
	  * TODO: Would it make more sense to treat Text 2 as 480 pixels?
	  */
	[[nodiscard]] constexpr unsigned getLineWidth() const {
		// Note: Testing "mode" instead of "base mode" ensures that YJK
		//       modes are treated as 256 pixels wide.
		return mode == one_of(TEXT2, GRAPHIC5, GRAPHIC6)
			? 512
			: 256;
	}
};

} // namespace openmsx

#endif
