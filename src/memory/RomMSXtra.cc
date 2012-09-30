//

#include "RomMSXtra.hh"
#include "Rom.hh"
#include "Ram.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

namespace openmsx {

RomMSXtra::RomMSXtra(const DeviceConfig& config, std::auto_ptr<Rom> rom)
	: MSXRom(config, rom)
	, ram(new Ram(config, getName() + " RAM", "MSXtra RAM",
	              0x0800))
{
	byte j[2]; j[0] = 0xa5; j[1] = 0x5a;
	for (int i=0;i<0x800;i++) (*ram)[i] = j[i&1];
}

RomMSXtra::~RomMSXtra()
{
}

byte RomMSXtra::readMem(word address, EmuTime::param /*time*/)
{
	if ((0x4000 <= address) && (address < 0x6000)) {
		return (*rom)[address & 0x1fff];
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		return (*ram)[address & 0x07ff];
	} else {
		return 0xff;
	}
}

const byte* RomMSXtra::getReadCacheLine(word address) const
{
	if ((0x4000 <= address) && (address < 0x6000)) {
		return &(*rom)[address & 0x1fff];
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		return &(*ram)[address & 0x07ff];
	} else {
		return unmappedRead;
	}
}

void RomMSXtra::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		(*ram)[address & 0x07ff] = value;
	}
}

byte* RomMSXtra::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		return const_cast<byte*>(&(*ram)[address & 0x07ff]);
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void RomMSXtra::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ram", *ram);
}
INSTANTIATE_SERIALIZE_METHODS(RomMSXtra);
REGISTER_MSXDEVICE(RomMSXtra, "RomMSXtra");

} // namespace
