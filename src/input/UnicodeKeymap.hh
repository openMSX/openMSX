#ifndef UNICODEKEYMAP_HH
#define UNICODEKEYMAP_HH

#include "openmsx.hh"
#include "string_ref.hh"
#include <vector>
#include <utility>
#include <cassert>

namespace openmsx {

class UnicodeKeymap
{
public:
	struct KeyInfo {
		KeyInfo(byte row_, byte keymask_, byte modmask_)
			: row(row_), keymask(keymask_), modmask(modmask_)
		{
			if (keymask == 0) {
				assert(row     == 0);
				assert(modmask == 0);
			}
		}
		KeyInfo()
			: row(0), keymask(0), modmask(0)
		{
		}
		byte row, keymask, modmask;
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
	byte relevantMods[0x100];
	KeyInfo deadKeys[NUM_DEAD_KEYS];
	const KeyInfo emptyInfo;
};

} // namespace openmsx

#endif
