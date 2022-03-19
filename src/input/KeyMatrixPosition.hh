#ifndef KEYMATRIXPOSITION_HH
#define KEYMATRIXPOSITION_HH

#include "narrow.hh"

#include <cassert>
#include <cstdint>

namespace openmsx {

/** A position (row, column) in a keyboard matrix.
  */
class KeyMatrixPosition final {
	static constexpr uint8_t INVALID = 0xFF;

public:
	/** Rows are in the range [0..NUM_ROWS). */
	static constexpr unsigned NUM_ROWS = 16;
	/** Columns are in the range [0..NUM_COLS). */
	static constexpr unsigned NUM_COLS = 8;
	/** Combined row and column values are in the range [0..NUM_ROWCOL). */
	static constexpr unsigned NUM_ROWCOL = 1 << 7;

	/** Creates an invalid key matrix position, which can be used when
	  * a key does not exist on a particular keyboard.
	  */
	constexpr KeyMatrixPosition() = default;

	/** Creates a key matrix position from a uint8_t: the row is stored in
	  * the high nibble, the column is stored in the low nibble.
	  */
	explicit constexpr KeyMatrixPosition(uint8_t rowCol_)
		: KeyMatrixPosition(rowCol_ >> 4, rowCol_ & 0x0F)
	{
	}

	/** Creates a key matrix position with a given row and column.
	  */
	constexpr KeyMatrixPosition(unsigned row, unsigned col)
		: rowCol(narrow<uint8_t>((row << 3) | col))
	{
		assert(row < NUM_ROWS);
		assert(col < NUM_COLS);
		assert(isValid());
	}

	/** Returns true iff this position is valid.
	  */
	[[nodiscard]] constexpr bool isValid() const {
		return rowCol != INVALID;
	}

	/** Returns the matrix row.
	  * Must only be called on valid positions.
	  */
	[[nodiscard]] constexpr uint8_t getRow() const {
		assert(isValid());
		return rowCol >> 3;
	}

	/** Returns the matrix column.
	  * Must only be called on valid positions.
	  */
	[[nodiscard]] constexpr uint8_t getColumn() const {
		assert(isValid());
		return rowCol & 0x07;
	}

	/** Returns the matrix row and column combined in a single uint8_t: the column
	  * is stored in the lower 3 bits, the row is stored in the higher bits.
	  * Must only be called on valid positions.
	  */
	[[nodiscard]] constexpr uint8_t getRowCol() const {
		assert(isValid());
		return rowCol;
	}

	/** Returns a mask with the bit corresponding to this position's
	  * column set, all other bits clear.
	  * Must only be called on valid positions.
	  */
	[[nodiscard]] constexpr uint8_t getMask() const {
		assert(isValid());
		return uint8_t(1 << getColumn());
	}

	[[nodiscard]] constexpr bool operator==(const KeyMatrixPosition&) const = default;

private:
	uint8_t rowCol = INVALID;
};

} // namespace openmsx

#endif
