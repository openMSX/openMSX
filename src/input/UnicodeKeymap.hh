#ifndef UNICODEKEYMAP_HH
#define UNICODEKEYMAP_HH

#include "KeyMatrixPosition.hh"

#include <array>
#include <cassert>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace openmsx {

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

	explicit UnicodeKeymap(std::string_view extension);

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
};

} // namespace openmsx

#endif
