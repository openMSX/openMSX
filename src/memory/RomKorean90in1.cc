// $Id$

// Korean 90-in-1 cartridge
//
//  90 in 1 uses Port &H77 for mapping:
//    bits 0-5: selected 16KB page
//    bits 6-7: addressing mode...
//      00 = same page at 4000-7FFF and 8000-BFFF (normal mode)
//      01 = same page at 4000-7FFF and 8000-BFFF (normal mode)
//      10 = [page AND 3E] at 4000-7FFF, [page AND 3E OR 01] at 8000-BFFF
//           (32KB mode)
//      11 = same page at 4000-7FFF and 8000-BFFF, but 8000-BFFF has high 8KB
//           and low 8KB swapped (Namco mode)


#include "RomKorean90in1.hh"
#include "MSXCPUInterface.hh"
#include "Rom.hh"

namespace openmsx {

RomKorean90in1::RomKorean90in1(const XMLElement& config, const EmuTime& time,
                               std::auto_ptr<Rom> rom)
	: Rom8kBBlocks(config, time, rom)
{
	reset(time);
	MSXCPUInterface::instance().register_IO_Out(0x77, this);
}

RomKorean90in1::~RomKorean90in1()
{
	MSXCPUInterface::instance().unregister_IO_Out(0x77, this);
}

void RomKorean90in1::reset(const EmuTime& time)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);
	writeIO(0x77, 0, time);
}

void RomKorean90in1::writeIO(byte /*port*/, byte value, const EmuTime& /*time*/)
{
	byte page = 2 * (value & 0x3F);
	switch (value & 0xC0) {
		case 0x00:
		case 0x40:
			setRom(2, page + 0); 
			setRom(3, page + 1); 
			setRom(4, page + 0); 
			setRom(5, page + 1); 
			break;
		case 0x80:
			setRom(2, (page & ~2) + 0); 
			setRom(3, (page & ~2) + 1); 
			setRom(4, (page |  2) + 0); 
			setRom(5, (page |  2) + 1); 
			break;
		case 0xC0:
			setRom(2, page + 0); 
			setRom(3, page + 1); 
			setRom(4, page + 1); 
			setRom(5, page + 0); 
			break;
		default:
			assert(false);
	}
}

byte* RomKorean90in1::getWriteCacheLine(word /*address*/) const
{
	return unmappedWrite;
}

} // namespace openmsx
