#ifndef UNICODEKEYMAP_HH
#define UNICODEKEYMAP_HH

#include "openmsx.hh"
#include "string_ref.hh"
#include <vector>
#include <utility>
#include <cassert>

namespace openmsx {

/** A position (row, column) in a keyboard matrix.
  */
class KeyMatrixPosition final {
	static constexpr unsigned INVALID = 0xFF;

public:
	/** Rows are in the range [0..NUM_ROWS). */
	static constexpr unsigned NUM_ROWS = 16;
	/** Columns are in the range [0..NUM_COLS). */
	static constexpr unsigned NUM_COLS = 8;
	/** Combined row and column values are in the range [0..NUM_ROWCOL). */
	static constexpr unsigned NUM_ROWCOL = 1 << 8;

	/** Creates an invalid key matrix position, which can be used when
	  * a key does not exist on a particular keyboard.
	  */
	KeyMatrixPosition()
		: rowCol(INVALID)
	{
	}

	/** Creates a key matrix position from a byte: the row is stored in
	  * the high nibble, the column is stored in the low nibble.
	  */
	KeyMatrixPosition(byte rowCol_)
		: rowCol(rowCol_)
	{
		assert(isValid());
	}

	/** Creates a key matrix position with a given row and column.
	  */
	KeyMatrixPosition(unsigned row, unsigned col)
		: rowCol((row << 4) | col)
	{
		assert(isValid());
	}

	/** Returns true iff this position is valid.
	  */
	bool isValid() const {
		return rowCol != INVALID;
	}

	/** Returns the matrix row.
	  * Must only be called on valid positions.
	  */
	unsigned getRow() const {
		assert(isValid());
		return rowCol >> 4;
	}

	/** Returns the matrix column.
	  * Must only be called on valid positions.
	  */
	unsigned getColumn() const {
		assert(isValid());
		return rowCol & 0x0F;
	}

	/** Returns the matrix row and column combined in a single byte: the row
	  * is stored in the high nibble, the column is stored in the low nibble.
	  * Must only be called on valid positions.
	  */
	byte getRowCol() const {
		assert(isValid());
		return rowCol;
	}

	/** Returns a mask with the bit corresponding to this position's
	  * column set, all other bits clear.
	  * Must only be called on valid positions.
	  */
	unsigned getMask() const {
		assert(isValid());
		return 1 << getColumn();
	}

	bool operator==(const KeyMatrixPosition& other) const {
		return rowCol == other.rowCol;
	}

private:
	byte rowCol;
};

class UnicodeKeymap
{
public:
	struct KeyInfo {
		// Modifier masks:
		static constexpr byte SHIFT_MASK = 0x01;
		static constexpr byte CTRL_MASK  = 0x02;
		static constexpr byte GRAPH_MASK = 0x04;
		static constexpr byte CAPS_MASK  = 0x08;
		static constexpr byte CODE_MASK  = 0x10;

		KeyInfo(KeyMatrixPosition pos_, byte modmask_)
			: pos(pos_), modmask(modmask_)
		{
			assert(pos.isValid());
		}
		KeyInfo()
			: pos(), modmask(0)
		{
		}
		bool isValid() const {
			return pos.isValid();
		}
		KeyMatrixPosition pos;
		byte modmask;
	};

	explicit UnicodeKeymap(string_ref keyboardType);

	KeyInfo get(int unicode) const;
	KeyInfo getDeadkey(unsigned n) const;

	/** Checks whether the given key press needs a different lock key state.
	  * @param keyInfo The key to be pressed.
	  * @param modmask The mask that identifies the lock key: CODE/GRAPH/CAPS.
	  * @param lockOn The current state of the lock key.
	  * @return True if the lock key state needs to be toggled, false if the
	  *         lock key state is as required or does not matter.
	  */
	bool needsLockToggle(const KeyInfo& keyInfo, byte modmask, bool lockOn) const;

private:
	static const unsigned NUM_DEAD_KEYS = 3;

	void parseUnicodeKeymapfile(const char* begin, const char* end);

	std::vector<std::pair<int, KeyInfo>> mapdata;
	/** Contains a mask for each key matrix position, which for each modifier
	  * has the corresponding bit set if that modifier that affects the key.
	  */
	byte relevantMods[KeyMatrixPosition::NUM_ROWCOL];
	KeyInfo deadKeys[NUM_DEAD_KEYS];
};

} // namespace openmsx

#endif
