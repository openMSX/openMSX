// $Id$

#include <algorithm>
#include "StringOp.hh"
#include "Config.hh"
#include "RomPlain.hh"

using std::min;

namespace openmsx {

RomPlain::RomPlain(Config* config, const EmuTime& time, Rom* rom)
	: MSXDevice(config, time), Rom8kBBlocks(config, time, rom)
{
	switch (rom->getSize()) {
		case 0x2000:	//  8kB
			for (int i = 0; i < 8; i++) {
				setRom(i, 0);
			}
		case 0x4000:	// 16kB
			for (int i = 0; i < 8; i++) {
				setRom(i, i & 1);
			}
			break;
		case 0x8000:	// 32kB
			if (guessLocation() & 0x4000) {
				for (int i = 0; i < 4; i++) {
					setRom(i, i & 1);
				}
				for (int i = 4; i < 8; i++) {
					setRom(i, 2 + (i & 1));
				}
			} else {
				for (int i = 0; i < 8; i++) {
					setRom(i, i & 3);
				}
			}
			break;
		case 0xC000:	// 48kB
			if (guessLocation() & 0x4000) {
				setRom(0, 0);
				setRom(1, 1);
				for (int i = 0; i < 6; i++) {
					setRom(i + 2, i);
				}
			} else {
				for (int i = 0; i < 6; i++) {
					setRom(i, i);
				}
				setRom(6, 4);
				setRom(7, 5);
			}
			break;
		case 0x10000:	// 64kB
			for (int i = 0; i < 8; i++) {
				setRom(i, i);
			}
			break;
		default:
			throw FatalError("Romsize must be exactly 8, 16, 32, 48 "
			                 "or 64kB for mappertype PLAIN");
	}
}

RomPlain::~RomPlain()
{
}

void RomPlain::guessHelper(word offset, int* pages)
{
	if ((rom->read(offset++) == 'A') && (rom->read(offset++) =='B')) {
		for (int i = 0; i < 4; i++) {
			word addr = rom->read(offset++) +
			            rom->read(offset++) * 256;
			if (addr) {
				int page = (addr >> 14) - (offset >> 14);
				if ((0 <= page) && (page <= 2)) {
					pages[page]++;
				}
			}
		}
	}
}

word RomPlain::guessLocation()
{
	int pages[3] = { 0, 0, 0 };

	// count number of possible routine pointers
	if (rom->getSize() >= 0x0010) {
		guessHelper(0x0000, pages);
	}
	if (rom->getSize() >= 0x4010) {
		guessHelper(0x4000, pages);
	}
	PRT_DEBUG("MSXRom: location " << pages[0] << 
	                          " " << pages[1] <<
				  " " << pages[2]);
	// we prefer 0x4000, then 0x000 and then 0x8000
	if (pages[1] && (pages[1] >= pages[0]) && (pages[1] >= pages[2])) {
		return 0x4000;
	} else if (pages[0] && pages[0] >= pages[2]) {
		return 0x0000;
	} else if (pages[2]) {
		return 0x8000;
	}

	int lowest = 4;
	const XMLElement::Children& children =
		deviceConfig->getXMLElement().getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == "slotted") {
			const XMLElement::Children& slot_children = (*it)->getChildren();
			for (XMLElement::Children::const_iterator it2 = slot_children.begin();
			     it2 != slot_children.end(); ++it2) {
				if ((*it2)->getName() == "page") {
					int page = StringOp::stringToInt((*it2)->getPcData());
					lowest = min(lowest, page);
				}
			}
		}
	}

	return (lowest * 0x4000) & 0xFFFF;
}

} // namespace openmsx
