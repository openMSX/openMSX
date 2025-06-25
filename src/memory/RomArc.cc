// Parallax' Arc

#include "RomArc.hh"
#include "MSXCPUInterface.hh"
#include "serialize.hh"

namespace openmsx {

RomArc::RomArc(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	setUnmapped(0);
	setRom(1, 0);
	setRom(2, 1);
	setUnmapped(3);

	reset(EmuTime::dummy());

	getCPUInterface().register_IO_InOut(0x7f, this);
}

RomArc::~RomArc()
{
	getCPUInterface().unregister_IO_InOut(0x7f, this);
}

void RomArc::reset(EmuTime /*time*/)
{
	offset = 0x00;
}

void RomArc::writeIO(uint16_t /*port*/, byte value, EmuTime /*time*/)
{
	if (value == 0x35) {
		++offset;
	}
}

byte RomArc::readIO(uint16_t port, EmuTime time)
{
	return RomArc::peekIO(port, time);
}

byte RomArc::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	return ((offset & 0x03) == 0x03) ? 0xda : 0xff;
}

template<typename Archive>
void RomArc::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("offset", offset);
}
INSTANTIATE_SERIALIZE_METHODS(RomArc);
REGISTER_MSXDEVICE(RomArc, "RomArc");

} // namespace openmsx
