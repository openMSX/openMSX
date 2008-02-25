// $Id: KeyboardSettings.hh 7235 2007-11-04 21:24:10Z awulms $

#ifndef UNICODEKEYMAP_HH
#define UNICODEKEYMAP_HH

#include "openmsx.hh"
#include <string>
#include <map>

namespace openmsx {

class UnicodeKeymap
{
public:
	UnicodeKeymap(std::string& keyboardType);
	struct KeyInfo {
		KeyInfo(byte row_, byte keymask_, byte modmask_)
			:row(row_),keymask(keymask_),modmask(modmask_) {}
		KeyInfo() {}
		byte row; byte keymask; byte modmask;
	};
	KeyInfo get(int unicode);
	KeyInfo getDeadkey();

private:
	void parseUnicodeKeymapfile(const openmsx::byte*, unsigned int);
	std::map<int, KeyInfo> mapdata;
	KeyInfo emptyInfo;
	KeyInfo deadKey;
};


} // namespace openmsx

#endif
