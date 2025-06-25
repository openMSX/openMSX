// Retrohard 31-in-1 cartridge
//
//  31 in 1 cartridges use Port &H94 for mapping 32kB games at their respective
//  offsets into 0x4000-0xBFFF

#include "RomRetroHard31in1.hh"
#include "MSXCPUInterface.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

RomRetroHard31in1::RomRetroHard31in1(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
	getCPUInterface().register_IO_Out(0x94, this);
}

RomRetroHard31in1::~RomRetroHard31in1()
{
	getCPUInterface().unregister_IO_Out(0x94, this);
}

void RomRetroHard31in1::reset(EmuTime time)
{
	writeIO(0x94, 0, time);
}

void RomRetroHard31in1::writeIO(uint16_t /*port*/, byte value, EmuTime /*time*/)
{
	byte page = 2 * (value & 0x1F);
	// note: we use a mirrored configuration here on purpose.
	// Without this, H.E.R.O. doesn't work from the dump I got.
	setRom(0, page + 1);
	setRom(1, page + 0);
	setRom(2, page + 1);
	setRom(3, page + 0);
}

byte* RomRetroHard31in1::getWriteCacheLine(uint16_t /*address*/)
{
	return unmappedWrite.data();
}

REGISTER_MSXDEVICE(RomRetroHard31in1, "RomRetroHard31in1");

} // namespace openmsx
