// $Id$

#include "RomPlain.hh"
#include "MSXConfig.hh"


RomPlain::RomPlain(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom16kBBlocks(config, time)
{
	switch (rom.getSize()) {
		case 0x4000:	// 16kB
			for (int i = 0; i < 4; i++) {
				setRom(i, 0);
			}
			break;
		case 0x8000:	// 32kB
			if (guessLocation() & 0x4000) {
				setRom(0, 0);
				setRom(1, 0);
				setRom(2, 1);
				setRom(3, 1);
			} else {
				setRom(0, 0);
				setRom(1, 1);
				setRom(2, 0);
				setRom(3, 1);
			}
			break;
		case 0xC000:	// 48kB
			if (guessLocation() & 0x4000) {
				setRom(0, 0);
				setRom(1, 0);
				setRom(2, 1);
				setRom(3, 2);
			} else {
				setRom(0, 0);
				setRom(1, 1);
				setRom(2, 2);
				setRom(3, 2);
			}
			break;
		case 0x10000:	// 64kB
			for (int i = 0; i < 4; i++) {
				setRom(i, i);
			}
			break;
		default:
			PRT_ERROR("Romsize must be exactly 16, 32, 48 or 64kB "
			          "for mappertype PLAIN");
	}
}

RomPlain::~RomPlain()
{
}


void RomPlain::guessHelper(word offset, int* pages)
{
	if ((rom.read(offset++) == 'A') && (rom.read(offset++) =='B')) {
		for (int i = 0; i < 4; i++) {
			word addr = rom.read(offset++) +
			            rom.read(offset++) * 256;
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
	if (rom.getSize() >= 0x0010) {
		guessHelper(0x0000, pages);
	}
	if (rom.getSize() >= 0x4010) {
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
	std::list<Device::Slotted*>::const_iterator i;
	for (i =  deviceConfig->slotted.begin(); 
	     i != deviceConfig->slotted.end();
	     i++) {
		int page = (*i)->getPage();
		if (page < lowest) lowest = page;
	}
	return (lowest * 0x4000) & 0xFFFF;
}
