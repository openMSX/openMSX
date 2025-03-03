#ifndef UNICODEKEYMAP_HH
#define UNICODEKEYMAP_HH

#include "CommandException.hh"
#include "MsxChar2Unicode.hh"

#include "narrow.hh"

#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

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

class UnicodeKeymap
{
public:
	struct KeyInfo {
		enum class Modifier : uint8_t { SHIFT, CTRL, GRAPH, CAPS, CODE, NUM };
		// Modifier masks:
		static constexpr uint8_t SHIFT_MASK = 1 << std::to_underlying(Modifier::SHIFT);
		static constexpr uint8_t CTRL_MASK  = 1 << std::to_underlying(Modifier::CTRL);
		static constexpr uint8_t GRAPH_MASK = 1 << std::to_underlying(Modifier::GRAPH);
		static constexpr uint8_t CAPS_MASK  = 1 << std::to_underlying(Modifier::CAPS);
		static constexpr uint8_t CODE_MASK  = 1 << std::to_underlying(Modifier::CODE);

		constexpr KeyInfo() = default;
		constexpr KeyInfo(KeyMatrixPosition pos_, uint8_t modMask_)
			: pos(pos_), modMask(modMask_)
		{
			assert(pos.isValid());
		}
		[[nodiscard]] constexpr bool isValid() const {
			return pos.isValid();
		}
		KeyMatrixPosition pos;
		uint8_t modMask = 0;
	};

	explicit UnicodeKeymap(std::string_view keyboardType);

	[[nodiscard]] KeyInfo get(unsigned unicode) const;
	[[nodiscard]] KeyInfo getDeadKey(unsigned n) const;

	/** Returns a mask in which a bit is set iff the corresponding modifier
	  * is relevant for the given key. A modifier is considered relevant if
	  * there is at least one mapping entry for the key that requires the
	  * modifier to be active.
	  * Must only be called on valid KeyInfos.
	  */
	[[nodiscard]] uint8_t getRelevantMods(const KeyInfo& keyInfo) const {
		return relevantMods[keyInfo.pos.getRowCol()];
	}

	[[nodiscard]] const MsxChar2Unicode& getMsxChars() const {
		if (!msxChars) throw CommandException("Missing MSX-Video-characterset file"); // TODO make this required for MSX/SVI machines
		return *msxChars;
	}

private:
	static constexpr unsigned NUM_DEAD_KEYS = 3;

	void parseUnicodeKeyMapFile(std::string_view data);

private:
	struct Entry {
		unsigned unicode;
		KeyInfo keyInfo;
	};
	std::vector<Entry> mapData; // sorted on unicode

	/** Contains a mask for each key matrix position, which for each modifier
	  * has the corresponding bit set if that modifier that affects the key.
	  */
	std::array<uint8_t, KeyMatrixPosition::NUM_ROWCOL> relevantMods;
	std::array<KeyInfo, NUM_DEAD_KEYS> deadKeys;

	std::optional<MsxChar2Unicode> msxChars; // TODO should this be required for MSX/SVI machines?
};

} // namespace openmsx

#endif
