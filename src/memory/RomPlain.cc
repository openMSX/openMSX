// $Id$

#include <algorithm>
#include "StringOp.hh"
#include "RomPlain.hh"
#include "Rom.hh"
#include "xmlx.hh"

using std::min;

namespace openmsx {

RomPlain::RomPlain(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom_)
	: Rom8kBBlocks(config, time, rom_)
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
			if (guessLocation(config) & 0x4000) {
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
			if (guessLocation(config) & 0x4000) {
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
	if (((*rom)[offset++] == 'A') && ((*rom)[offset++] =='B')) {
		for (int i = 0; i < 4; i++) {
			word addr = (*rom)[offset++] +
			            (*rom)[offset++] * 256;
			if (addr) {
				int page = (addr >> 14) - (offset >> 14);
				if ((0 <= page) && (page <= 2)) {
					pages[page]++;
				}
			}
		}
	}
}

word RomPlain::guessLocation(const XMLElement& config)
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

	const XMLElement* mem = config.findChild("mem");
	if (mem) {
		return mem->getAttributeAsInt("base");
	} else {
		return 0;
	}
}

} // namespace openmsx
