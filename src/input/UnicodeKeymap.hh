// $Id$

#ifndef UNICODEKEYMAP_HH
#define UNICODEKEYMAP_HH

#include "openmsx.hh"
#include <string>
#include <map>
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
		{
			KeyInfo(0, 0, 0);
		}
		byte row, keymask, modmask;
	};

	explicit UnicodeKeymap(const std::string& keyboardType);

	KeyInfo get(int unicode) const;
	KeyInfo getDeadkey(int n) const;

private:
	void parseUnicodeKeymapfile(const char* begin, const char* end);

	typedef std::map<int, KeyInfo> Mapdata;
	Mapdata mapdata;
	KeyInfo *deadKeys;
	const KeyInfo emptyInfo;
	const int nDeadKeys;
};

} // namespace openmsx

#endif
